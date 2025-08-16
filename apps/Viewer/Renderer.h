/*
 * Renderer.h
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

#pragma once

#include "Camera.h"
#include "Shader.h"
#include "BufferObjects.h"
#include "Image.h"

namespace VIEWER {

// Forward declarations
class Window;

struct ViewProjectionData {
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
	Eigen::Matrix4f view;
	Eigen::Matrix4f projection;
	Eigen::Matrix4f viewProjection;
	Eigen::Vector3f cameraPos;
	float padding; // Alignment
};

struct LightingData {
	Eigen::Vector3f lightDirection;
	float lightIntensity;
	Eigen::Vector3f lightColor;
	float ambientStrength;
	Eigen::Vector3f ambientColor;
	float padding; // Alignment
};

class Renderer {
private:
	// Uniform Buffer Objects
	std::unique_ptr<UBO> viewProjectionUBO;
	std::unique_ptr<UBO> lightingUBO;

	// Point cloud rendering
	std::unique_ptr<Shader> pointCloudShader;
	std::unique_ptr<Shader> pointCloudNormalsShader;
	std::unique_ptr<VAO> pointCloudVAO;
	std::unique_ptr<VBO> pointCloudVBO, pointCloudColorVBO;
	std::unique_ptr<VAO> pointCloudNormalsVAO;
	std::unique_ptr<VBO> pointCloudNormalsVBO;
	size_t pointCount;
	size_t pointNormalCount;

	// Mesh rendering
	std::unique_ptr<Shader> meshShader;
	std::unique_ptr<Shader> meshTexturedShader;
	std::unique_ptr<VAO> meshVAO;
	std::unique_ptr<VBO> meshVBO, meshEBO, meshNormalVBO, meshTexCoordVBO;
	MVS::Mesh::FaceIdxArr mapFaceSubsetIndices;
	std::vector<size_t> meshFaceCounts;
	std::vector<Image> meshTextures;

	// Geometry selection highlighting (for SelectionController)
	std::unique_ptr<Shader> geometrySelectionShader;

	// Camera frustum rendering
	std::unique_ptr<Shader> cameraShader;
	std::unique_ptr<VAO> cameraVAO;
	std::unique_ptr<VBO> cameraVBO, cameraEBO, cameraColorVBO;
	size_t cameraIndexCount;

	// 3D image overlay rendering (pre-computed for all images with valid textures)
	std::unique_ptr<Shader> imageOverlayShader;
	std::unique_ptr<VAO> imageOverlayVAO;
	std::unique_ptr<VBO> imageOverlayVBO;
	std::unique_ptr<VBO> imageOverlayEBO;
	size_t imageOverlayIndexCount;

	// Selection rendering
	std::unique_ptr<Shader> selectionShader;
	std::unique_ptr<VAO> selectionVAO;
	std::unique_ptr<VBO> selectionVBO;
	size_t selectionPrimitiveCount;

	// Selection overlay rendering (2D screen space)
	std::unique_ptr<Shader> selectionOverlayShader;
	std::unique_ptr<VAO> selectionOverlayVAO;
	std::unique_ptr<VBO> selectionOverlayVBO;
	size_t selectionOverlayVertexCount;

	// Bounds rendering
	std::unique_ptr<Shader> boundsShader;
	std::unique_ptr<VAO> boundsVAO;
	std::unique_ptr<VBO> boundsVBO;
	size_t boundsPrimitiveCount;

	// Coordinate axes
	std::unique_ptr<Shader> axesShader;
	std::unique_ptr<VAO> axesVAO;
	std::unique_ptr<VBO> axesVBO, axesColorVBO;

	// Arcball gizmos (combined buffer for circles and center axes)
	std::unique_ptr<Shader> gizmoShader;
	std::unique_ptr<VAO> gizmoVAO;
	std::unique_ptr<VBO> gizmoVBO, gizmoEBO;
	size_t gizmoCircleIndexCount; // Circle rendering indices
	size_t gizmoCenterAxesBaseVertex; // Starting vertex for center axes
	size_t gizmoCenterAxesVertexCount; // Number of center axes vertices

public:
	Renderer();
	~Renderer();

	bool Initialize();
	void Release();
	void Reset();

	// Data upload
	void UploadPointCloud(const MVS::PointCloud& pointcloud, float normalLength);
	void UploadMesh(MVS::Mesh& mesh);
	void UploadCameras(const Window& window);
	void UploadImageOverlays(const Window& window);
	void UploadSelection(const Window& window);
	void UploadBounds(const MVS::Scene& scene);

	// Rendering
	void BeginFrame(const Camera& camera, const Eigen::Vector4f& clearColor);
	void SetLighting(const Eigen::Vector3f& direction, float intensity, const Eigen::Vector3f& color);

	void RenderPointCloud(const Window& window);
	void RenderPointCloudNormals(const Window& window);
	void RenderMesh(const Window& window);
	void RenderCameras(const Window& window);
	void RenderImageOverlays(const Window& window);
	void RenderSelection(const Window& window);
	void RenderSelectionOverlay(const Window& window);
	void RenderSelectedGeometry(const Window& window);
	void RenderBounds();
	void RenderCoordinateAxes(const Camera& camera);
	void RenderArcballGizmos(const Camera& camera, const class ArcballControls& controls);

	void EndFrame();

	// Getters
	size_t GetMeshSubMeshCount() const { return meshFaceCounts.size(); }

private:
	void CreateShaders();
	void CreateBuffers();
	void UpdateViewProjection(const Camera& camera);
	void UpdateLighting();

	// Utility methods
	void SetupPointCloudBuffers();
	void SetupPointCloudNormalsBuffers();
	void SetupMeshBuffers();
	void SetupCameraBuffers();
	void SetupImageOverlayBuffers();
	void SetupSelectionBuffers();
	void SetupSelectionOverlayBuffers();
	void SetupBoundsBuffers();
	void SetupAxesBuffers();
	void SetupGizmoBuffers();
};
/*----------------------------------------------------------------*/

} // namespace VIEWER
