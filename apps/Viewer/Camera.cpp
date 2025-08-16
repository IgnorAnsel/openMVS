/*
 * Camera.cpp
 *
 * Copyright (c) 2014-2025 SEACAVE
 *
 * Author(s):
 *
 *      cDc <cdc.seacave@gmail.com>
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Additional Terms:
 *
 *      You are required to preserve legal notices and author attributions in
 *      that material or in the Appropriate Legal Notices displayed by works
 *      containing it.
 */

#include "Common.h"
#include "Camera.h"
#include "Window.h"

using namespace VIEWER;

Camera::Camera()
	: position(0, 0, 5)
	, target(0, 0, 0)
	, up(0, 1, 0)
	, size(800, 600)
	, fov(45.0)
	, nearPlane(0.1)
	, farPlane(1000.0)
	, orthographic(false)
	, prevCamID(NO_ID)
	, currentCamID(NO_ID)
	, maxCamID(NO_ID)
{
}

void Camera::SetFOV(double newFov) {
	fov = CLAMP(newFov, 1.0, 179.0);
	Window::RequestRedraw();
}

void Camera::SetNearFar(double _nearPlane, double _farPlane) {
	nearPlane = _nearPlane;
	farPlane = _farPlane;
}

void Camera::SetOrthographic(bool ortho) {
	orthographic = ortho;
	Window::RequestRedraw();
}

Eigen::Matrix3d Camera::GetRotationMatrix() const {
	// Create camera rotation matrix
	Eigen::Vector3d viewDir = (target - position).normalized();
	Eigen::Vector3d right = viewDir.cross(up).normalized();
	Eigen::Vector3d up = right.cross(viewDir).normalized();

	Eigen::Matrix3d rotation;
	rotation.col(0) = right;
	rotation.col(1) = up;
	rotation.col(2) = -viewDir; // negative because OpenGL convention
	return rotation;
}

Eigen::Matrix4d Camera::GetViewMatrix() const {
	return ComputeLookAtMatrix(position, target, up);
}

Eigen::Matrix4d Camera::GetProjectionMatrix() const {
	double aspect = static_cast<double>(size.width) / static_cast<double>(size.height);
	if (orthographic) {
		// Calculate orthographic bounds based on distance to target
		double distance = (position - target).norm();
		double height = distance * TAN(fov * M_PI / 360.0); // Half height
		double width = height * aspect;

		Eigen::Matrix4d ortho = Eigen::Matrix4d::Zero();
		ortho(0,0) = 1.0 / width;
		ortho(1,1) = 1.0 / height;
		ortho(2,2) = -2.0 / (farPlane - nearPlane);
		ortho(2,3) = -(farPlane + nearPlane) / (farPlane - nearPlane);
		ortho(3,3) = 1.0;
		return ortho;
	} else {
		// Perspective projection
		double f = 1.0 / TAN(D2R(fov) * 0.5);

		Eigen::Matrix4d proj = Eigen::Matrix4d::Zero();
		proj(0,0) = f / aspect;
		proj(1,1) = f;
		proj(2,2) = (farPlane + nearPlane) / (nearPlane - farPlane);
		proj(2,3) = (2.0 * farPlane * nearPlane) / (nearPlane - farPlane);
		proj(3,2) = -1.0;
		return proj;
	}
}

void Camera::Reset() {
	// Calculate center and size
	const float maxSize = sceneSize.maxCoeff();

	// Position camera to view the entire scene
	double distance = maxSize / (2.0 * TAN(D2R(fov) * 0.5)) * 1.5; // 1.5x for padding

	savedState.reset();
	target = sceneCenter.cast<double>();
	position = sceneCenter.cast<double>() + Eigen::Vector3d(0, 0, distance);
	up = Eigen::Vector3d(0, 1, 0);

	// Set reasonable near/far planes
	nearPlane = MAXF(distance * 0.001, 0.001);
	farPlane = distance * 10.0;
	DisableCameraViewMode();

	// Request redraw when camera resets
	Window::RequestRedraw();
}

void Camera::SetSceneBounds(const Point3f& center, const Point3f& size) {
	sceneCenter = center;
	sceneSize = size;
	Reset();
}

void Camera::SetLookAt(const Eigen::Vector3d& eye, const Eigen::Vector3d& target, const Eigen::Vector3d& up) {
	position = eye;
	this->target = target;
	this->up = up.normalized();
	Window::RequestRedraw();
}

Ray3d Camera::GetPickingRay(const Eigen::Vector2d& screenPos) const {
	// screenPos is already normalized to [-1, 1] range from Window::NormalizeMousePos()
	Eigen::Vector4d rayClip(screenPos.x(), screenPos.y(), -1.0, 1.0);

	// Transform to eye coordinates
	Eigen::Matrix4d invProj = GetProjectionMatrix().inverse();
	Eigen::Vector4d rayEye = invProj * rayClip;
	rayEye = Eigen::Vector4d(rayEye.x(), rayEye.y(), -1.0, 0.0);

	// Transform to world coordinates
	Eigen::Matrix4d invView = GetViewMatrix().inverse();
	Eigen::Vector4d rayWorld = invView * rayEye;

	Eigen::Vector3d rayDirection = rayWorld.head<3>().normalized();

	return Ray3d(position, rayDirection);
}

// Static helper function for computing LookAt matrix
Eigen::Matrix4d Camera::ComputeLookAtMatrix(const Eigen::Vector3d& eye, const Eigen::Vector3d& center, const Eigen::Vector3d& up) {
	const Eigen::Vector3d n((center-eye).normalized());
	const Eigen::Vector3d s(n.cross(up));
	const Eigen::Vector3d v(s.cross(n));

	Eigen::Matrix4d m; m <<
		 s(0),  s(1),  s(2), -eye.dot(s),
		 v(0),  v(1),  v(2), -eye.dot(v),
		-n(0), -n(1), -n(2),  eye.dot(n),
		 0.0, 0.0, 0.0, 1.0;
	return m;
}

// Set camera view mode based on viewer camera ID
//  - camID: viewer camera index to switch to
void Camera::SetCameraViewMode(MVS::IIndex camID) {
	ASSERT(camID < maxCamID);
	// Use callback to request camera data from Scene
	if (cameraViewModeCallback)
		cameraViewModeCallback(camID);
}

void Camera::DisableCameraViewMode() {
	if (!IsCameraViewMode())
		return;
	prevCamID = currentCamID = NO_ID;
	RestoreSavedState();
}

void Camera::NextCamera() {
	if (maxCamID == NO_ID)
		return;
	const MVS::IIndex camID(currentCamID == NO_ID ? 0 : currentCamID + 1);
	if (camID < maxCamID)
		SetCameraViewMode(camID);
	else
		DisableCameraViewMode();
}

void Camera::PreviousCamera() {
	if (maxCamID == NO_ID)
		return;
	const MVS::IIndex camID(currentCamID == NO_ID ? maxCamID - 1 : currentCamID - 1);
	if (camID < maxCamID)
		SetCameraViewMode(camID);
	else
		DisableCameraViewMode();
}

void Camera::SetCameraFromSceneData(const MVS::Image& imageData) {
	ASSERT(imageData.IsValid());

	// Get camera position and orientation from MVS camera data
	position = imageData.camera.C;

	// Calculate target point by projecting forward from camera
	// In MVS camera coordinates: X=right, Y=down, Z=forward
	// We want to look in the +Z direction of the camera
	Eigen::Vector3d forward(imageData.camera.Direction());
	up = imageData.camera.UpDirection();
	// Set target point a reasonable distance forward
	double targetDistance = 1.0; // Can be adjusted based on scene scale
	target = position + forward * targetDistance;

	// Set FOV from camera intrinsics
	double fovY = R2D(imageData.ComputeFOV(1));

	// Adjust FOV based on viewport size
	double imageAspect = static_cast<double>(imageData.width) / imageData.height;
	double viewportAspect = static_cast<double>(size.width) / size.height;
	if (imageAspect > viewportAspect) {
		// Image is wider than viewport, adjust FOV to fit width
		fovY /= (imageAspect / viewportAspect);
	}
	SetFOV(fovY);
}

void Camera::SaveCurrentState() {
	CameraState state;
	state.position = position;
	state.target = target;
	state.up = up;
	state.fov = fov;
	state.size = size;
	state.orthographic = orthographic;
	savedState = state;
}

bool Camera::RestoreSavedState() {
	if (!savedState.has_value())
		return false;

	const CameraState& state = savedState.value();
	position = state.position;
	target = state.target;
	up = state.up;
	fov = state.fov;
	size = state.size;
	orthographic = state.orthographic;

	savedState.reset(); // Clear the saved state

	// Request redraw when restoring camera state
	Window::RequestRedraw();
	return true;
}
/*----------------------------------------------------------------*/
