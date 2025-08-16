/*
 * Renderer.cpp
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
#include "Renderer.h"
#include "Scene.h"

using namespace VIEWER;

Renderer::Renderer()
	: pointCount(0)
	, pointNormalCount(0)
	, cameraIndexCount(0)
	, imageOverlayIndexCount(0)
	, selectionPrimitiveCount(0)
	, selectionOverlayVertexCount(0)
	, boundsPrimitiveCount(0)
{
}

Renderer::~Renderer() {
}

bool Renderer::Initialize() {
	try {
		// Create uniform buffer objects first (binding points 0 and 1)
		viewProjectionUBO = std::make_unique<UBO>(0);
		lightingUBO = std::make_unique<UBO>(1);

		// Create shaders
		CreateShaders();

		// Create buffer objects
		CreateBuffers();

		// Set default lighting
		SetLighting(Eigen::Vector3f(0.f, 0.f, 1.f), 1.f, Eigen::Vector3f(1.f, 1.f, 1.f));

		// Enable depth testing
		GL_CHECK(glEnable(GL_DEPTH_TEST));
		GL_CHECK(glDepthFunc(GL_LESS));

		// Enable point size and line width control
		GL_CHECK(glEnable(GL_PROGRAM_POINT_SIZE));

		// Enable blending for transparency
		GL_CHECK(glEnable(GL_BLEND));
		GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

		return true;
	}
	catch (const std::exception& e) {
		DEBUG("Renderer initialization failed: %s", e.what());
		return false;
	}
}

void Renderer::Release() {
}

void Renderer::Reset() {
	// Reset scene-dependent resources for loading a new scene.
	// Clears all uploaded geometry data (point clouds, meshes, cameras, etc.)
	// while preserving scene-independent UI elements (gizmos, axes).

	// Reset scene-dependent primitive counts
	pointCount = 0;
	pointNormalCount = 0;
	cameraIndexCount = 0;
	imageOverlayIndexCount = 0;
	selectionPrimitiveCount = 0;
	boundsPrimitiveCount = 0;

	// Clear mesh-related data
	meshFaceCounts.clear();
	mapFaceSubsetIndices.clear();
	meshTextures.clear();

	// Clear scene-dependent geometry buffers by allocating empty data
	if (pointCloudVBO)
		pointCloudVBO->AllocateBuffer(0);
	if (pointCloudColorVBO)
		pointCloudColorVBO->AllocateBuffer(0);
	if (pointCloudNormalsVBO)
		pointCloudNormalsVBO->AllocateBuffer(0);

	if (meshVBO)
		meshVBO->AllocateBuffer(0);
	if (meshEBO)
		meshEBO->AllocateBuffer(0);
	if (meshNormalVBO)
		meshNormalVBO->AllocateBuffer(0);
	if (meshTexCoordVBO)
		meshTexCoordVBO->AllocateBuffer(0);

	if (cameraVBO)
		cameraVBO->AllocateBuffer(0);
	if (cameraEBO)
		cameraEBO->AllocateBuffer(0);
	if (cameraColorVBO)
		cameraColorVBO->AllocateBuffer(0);

	if (imageOverlayVBO)
		imageOverlayVBO->AllocateBuffer(0);
	if (imageOverlayEBO)
		imageOverlayEBO->AllocateBuffer(0);

	if (selectionVBO)
		selectionVBO->AllocateBuffer(0);

	if (boundsVBO)
		boundsVBO->AllocateBuffer(0);
}

void Renderer::CreateShaders() {
	// Point cloud shader
	pointCloudShader = std::make_unique<Shader>(
		#include "shaders/pointcloud.vert"
		,
		#include "shaders/pointcloud.frag"
	);

	// Point cloud normals shader
	pointCloudNormalsShader = std::make_unique<Shader>(
		#include "shaders/pointcloudnormals.vert"
		,
		#include "shaders/pointcloudnormals.frag"
	);

	// Mesh shader
	meshShader = std::make_unique<Shader>(
		#include "shaders/mesh.vert"
		,
		#include "shaders/mesh.frag"
	);

	// Mesh textured shader
	meshTexturedShader = std::make_unique<Shader>(
		#include "shaders/meshtextured.vert"
		,
		#include "shaders/meshtextured.frag"
	);

	// Geometry selection highlighting shader (for SelectionController)
	geometrySelectionShader = std::make_unique<Shader>(
		#include "shaders/geometryselection.vert"
		,
		#include "shaders/geometryselection.frag"
	);

	// Camera frustum shader
	cameraShader = std::make_unique<Shader>(
		#include "shaders/camera.vert"
		,
		#include "shaders/camera.frag"
	);

	// 3D Image overlay shader (renders textured quad in 3D world space)
	imageOverlayShader = std::make_unique<Shader>(
		#include "shaders/imageoverlay.vert"
		,
		#include "shaders/imageoverlay.frag"
	);

	// Selection shader (simple colored lines/points)
	selectionShader = std::make_unique<Shader>(
		#include "shaders/selection.vert"
		,
		#include "shaders/selection.frag"
	);

	// 2D overlay shader for SelectionController
	selectionOverlayShader = std::make_unique<Shader>(
		#include "shaders/selectionoverlay.vert"
		,
		#include "shaders/selectionoverlay.frag"
	);

	// Bounds shader
	boundsShader = std::make_unique<Shader>(
		#include "shaders/bounds.vert"
		,
		#include "shaders/bounds.frag"
	);

	// Coordinate axes shader
	axesShader = std::make_unique<Shader>(
		#include "shaders/axes.vert"
		,
		#include "shaders/axes.frag"
	);

	// Arcball gizmo shader
	gizmoShader = std::make_unique<Shader>(
		#include "shaders/gizmo.vert"
		,
		#include "shaders/gizmo.frag"
	);

	// Bind uniform buffer objects to shaders
	viewProjectionUBO->BindToShader(*pointCloudShader, "ViewProjection");
	viewProjectionUBO->BindToShader(*pointCloudNormalsShader, "ViewProjection");
	viewProjectionUBO->BindToShader(*meshShader, "ViewProjection");
	viewProjectionUBO->BindToShader(*meshTexturedShader, "ViewProjection");
	viewProjectionUBO->BindToShader(*geometrySelectionShader, "ViewProjection");
	viewProjectionUBO->BindToShader(*cameraShader, "ViewProjection");
	viewProjectionUBO->BindToShader(*imageOverlayShader, "ViewProjection");
	viewProjectionUBO->BindToShader(*selectionShader, "ViewProjection");
	viewProjectionUBO->BindToShader(*boundsShader, "ViewProjection");
	viewProjectionUBO->BindToShader(*gizmoShader, "ViewProjection");

	lightingUBO->BindToShader(*meshShader, "Lighting");
}

void Renderer::CreateBuffers() {
	SetupPointCloudBuffers();
	SetupPointCloudNormalsBuffers();
	SetupMeshBuffers();
	SetupCameraBuffers();
	SetupImageOverlayBuffers();
	SetupSelectionBuffers();
	SetupSelectionOverlayBuffers();
	SetupBoundsBuffers();
	SetupAxesBuffers();
	SetupGizmoBuffers();
}

void Renderer::SetupPointCloudBuffers() {
	pointCloudVAO = std::make_unique<VAO>();
	pointCloudVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);
	pointCloudColorVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);

	pointCloudVAO->Bind();

	// Position attribute (location 0)
	pointCloudVBO->Bind();
	pointCloudVAO->EnableAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	// Color attribute (location 1)
	pointCloudColorVBO->Bind();
	pointCloudVAO->EnableAttribute(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	pointCloudVAO->Unbind();
}

void Renderer::SetupPointCloudNormalsBuffers() {
	pointCloudNormalsVAO = std::make_unique<VAO>();
	pointCloudNormalsVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);

	pointCloudNormalsVAO->Bind();

	// Position attribute (location 0) - contains both start and end points of normal lines
	pointCloudNormalsVBO->Bind();
	pointCloudNormalsVAO->EnableAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	pointCloudNormalsVAO->Unbind();
}

void Renderer::SetupMeshBuffers() {
	meshVAO = std::make_unique<VAO>();
	meshVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);
	meshEBO = std::make_unique<VBO>(GL_ELEMENT_ARRAY_BUFFER);
	meshNormalVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);
	meshTexCoordVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);

	meshVAO->Bind();

	// Position attribute (location 0)
	meshVBO->Bind();
	meshVAO->EnableAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	// Normal attribute (location 1)
	meshNormalVBO->Bind();
	meshVAO->EnableAttribute(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	// Texture coordinate attribute (location 2)
	meshTexCoordVBO->Bind();
	meshVAO->EnableAttribute(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

	meshVAO->Unbind();
}

void Renderer::SetupCameraBuffers() {
	cameraVAO = std::make_unique<VAO>();
	cameraVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);
	cameraEBO = std::make_unique<VBO>(GL_ELEMENT_ARRAY_BUFFER);
	cameraColorVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);

	cameraVAO->Bind();

	// Position attribute (location 0)
	cameraVBO->Bind();
	cameraVAO->EnableAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	// Color attribute (location 1)
	cameraColorVBO->Bind();
	cameraVAO->EnableAttribute(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	cameraVAO->Unbind();
}

void Renderer::SetupSelectionBuffers() {
	selectionVAO = std::make_unique<VAO>();
	selectionVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);

	selectionVAO->Bind();

	// Position attribute (location 0)
	selectionVBO->Bind();
	selectionVAO->EnableAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	selectionVAO->Unbind();
}

void Renderer::SetupSelectionOverlayBuffers() {
	// Setup 2D overlay buffers for SelectionController
	selectionOverlayVAO = std::make_unique<VAO>();
	selectionOverlayVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);

	selectionOverlayVAO->Bind();
	selectionOverlayVBO->Bind();
	selectionOverlayVAO->EnableAttribute(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	selectionOverlayVAO->Unbind();

	selectionOverlayVertexCount = 0;
}

void Renderer::SetupBoundsBuffers() {
	boundsVAO = std::make_unique<VAO>();
	boundsVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);

	boundsVAO->Bind();

	// Position attribute (location 0)
	boundsVBO->Bind();
	boundsVAO->EnableAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	boundsVAO->Unbind();
}

void Renderer::SetupAxesBuffers() {
	axesVAO = std::make_unique<VAO>();
	axesVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);
	axesColorVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);

	axesVAO->Bind();

	// Position attribute (location 0)
	axesVBO->Bind();
	axesVAO->EnableAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	// Create coordinate axes data
	std::vector<float> axesVertices = {
		// X axis (red)
		0.f, 0.f, 0.f,
		1.f, 0.f, 0.f,
		// Y axis (green)
		0.f, 0.f, 0.f,
		0.f, 1.f, 0.f,
		// Z axis (blue)
		0.f, 0.f, 0.f,
		0.f, 0.f, 1.f
	};
	axesVBO->SetData(axesVertices);

	// Color attribute (location 1)
	axesColorVBO->Bind();
	axesVAO->EnableAttribute(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	std::vector<float> axesColors = {
		// X axis (red)
		1.f, 0.f, 0.f,
		1.f, 0.f, 0.f,
		// Y axis (green)
		0.f, 1.f, 0.f,
		0.f, 1.f, 0.f,
		// Z axis (blue)
		0.f, 0.f, 1.f,
		0.f, 0.f, 1.f
	};
	axesColorVBO->SetData(axesColors);

	axesVAO->Unbind();
}

void Renderer::SetupImageOverlayBuffers() {
	// Setup 3D image overlay buffers
	imageOverlayVAO = std::make_unique<VAO>();
	imageOverlayVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);
	imageOverlayEBO = std::make_unique<VBO>(GL_ELEMENT_ARRAY_BUFFER);

	imageOverlayVAO->Bind();

	// Bind the VBO before setting up attributes
	imageOverlayVBO->Bind();

	// Position attribute (location 0) - 3D world space coordinates
	imageOverlayVAO->EnableAttribute(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	// Texture coordinate attribute (location 1)
	imageOverlayVAO->EnableAttribute(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

	imageOverlayVAO->Unbind();
}

void Renderer::SetupGizmoBuffers() {
	// Setup combined gizmo buffers for both circles and center axes
	gizmoVAO = std::make_unique<VAO>();
	gizmoVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER);
	gizmoEBO = std::make_unique<VBO>(GL_ELEMENT_ARRAY_BUFFER);

	gizmoVAO->Bind();

	// Position attribute (location 0)
	gizmoVBO->Bind();
	gizmoVAO->EnableAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	// Generate circle geometry for trackball gizmos
	const int numSegments = 64;
	const float radius = 1.f;

	std::vector<float> vertices;
	std::vector<uint32_t> indices;

	// Generate vertices for a unit circle
	for (int i = 0; i <= numSegments; ++i) {
		float angle = FTWO_PI * i / numSegments;
		vertices.push_back(COS(angle) * radius); // x
		vertices.push_back(SIN(angle) * radius); // y
		vertices.push_back(0.f);                 // z
	}

	// Generate indices for line loop
	for (int i = 0; i < numSegments; ++i) {
		indices.push_back(i);
		indices.push_back(i + 1);
	}

	// Store index count for circles
	gizmoCircleIndexCount = indices.size();

	// Add center axes geometry (append to the same buffers)
	size_t centerAxesBaseVertex = vertices.size() / 3;
	std::vector<float> axesVertices = {
		// X axis
		0.f, 0.f, 0.f,
		1.f, 0.f, 0.f,
		// Y axis
		0.f, 0.f, 0.f,
		0.f, 1.f, 0.f,
		// Z axis
		0.f, 0.f, 0.f,
		0.f, 0.f, 1.f
	};
	vertices.insert(vertices.end(), axesVertices.begin(), axesVertices.end());

	// Store starting vertex for center axes (for rendering)
	gizmoCenterAxesBaseVertex = centerAxesBaseVertex;
	gizmoCenterAxesVertexCount = 6; // 3 axes, 2 vertices each

	// Upload combined geometry
	gizmoVBO->SetData(vertices);
	gizmoEBO->Bind();
	gizmoEBO->SetData(indices.data(), indices.size() * sizeof(uint32_t), GL_STATIC_DRAW);

	gizmoVAO->Unbind();
}

void Renderer::UploadPointCloud(const MVS::PointCloud& pointcloud, float normalLength) {
	pointCount = pointcloud.GetSize();
	pointNormalCount = 0;
	if (pointCount == 0)
		return;
	// Convert colors to float array
	std::vector<float> colors;
	colors.reserve(pointcloud.colors.size() * 3);
	if (!pointcloud.colors.empty()) {
		for (const auto& color : pointcloud.colors) {
			colors.push_back(color.r / 255.f);
			colors.push_back(color.g / 255.f);
			colors.push_back(color.b / 255.f);
		}
	} else {
		// Default white color for all points
		colors.resize(pointcloud.points.size() * 3, 1.f);
	}
	// Upload to GPU
	pointCloudVBO->SetData(pointcloud.points[0].ptr(), pointcloud.points.size() * 3);
	pointCloudColorVBO->SetData(colors);

	// Upload normals if available
	if (!pointcloud.normals.empty()) {
		ASSERT(pointcloud.normals.size() == pointcloud.points.size());
		// Create line segments for normals: each normal gets 2 vertices (start and end)
		std::vector<float> normalLines;
		normalLines.reserve(pointcloud.normals.size() * 6); // 2 points * 3 components each
		for (size_t i = 0; i < pointcloud.points.size(); ++i) {
			const MVS::PointCloud::Point& point = pointcloud.points[i];
			const MVS::PointCloud::Normal& normal = pointcloud.normals[i];
			// Start point (the actual point)
			normalLines.push_back(point.x);
			normalLines.push_back(point.y);
			normalLines.push_back(point.z);
			// End point (point + normal * length)
			normalLines.push_back(point.x + normal.x * normalLength);
			normalLines.push_back(point.y + normal.y * normalLength);
			normalLines.push_back(point.z + normal.z * normalLength);
		}
		pointCloudNormalsVBO->SetData(normalLines);
		pointNormalCount = normalLines.size() / 3; // Total vertices for normal lines
	}
}

void Renderer::UploadMesh(MVS::Mesh& mesh) {
	mapFaceSubsetIndices.clear();
	meshFaceCounts.clear();
	meshTextures.clear();
	if (mesh.IsEmpty())
		return;

	if (mesh.HasTexture()) {
		// Convert mesh to use texture per vertex and
		// split it in sub-meshes if multiple textures are present
		std::vector<MVS::Mesh> meshes;
		if (mesh.texturesDiffuse.size() > 1) {
			meshes = mesh.SplitMeshPerTextureBlob(&mapFaceSubsetIndices);
			for (MVS::Mesh& submesh: meshes) {
				MVS::Mesh convertedMesh;
				submesh.ConvertTexturePerVertex(convertedMesh);
				submesh.Swap(convertedMesh);
			}
		} else {
			MVS::Mesh convertedMesh;
			mesh.ConvertTexturePerVertex(convertedMesh);
			meshes.emplace_back(std::move(convertedMesh));
		}

		// Calculate total buffer sizes for all sub-meshes
		size_t totalVertices = 0;
		size_t totalIndices = 0;
		size_t totalTexCoords = 0;
		for (const MVS::Mesh& submesh : meshes) {
			totalVertices += submesh.vertices.size();
			totalIndices += submesh.faces.size() * 3;
			totalTexCoords += submesh.vertices.size(); // One tex coord per vertex after conversion
		}

		// Allocate total buffer sizes using VBO wrapper functions
		meshVBO->AllocateBuffer(totalVertices * 3 * sizeof(float));
		meshNormalVBO->AllocateBuffer(totalVertices * 3 * sizeof(float));
		meshTexCoordVBO->AllocateBuffer(totalTexCoords * 2 * sizeof(float));
		meshEBO->AllocateBuffer(totalIndices * sizeof(uint32_t));

		// Upload each sub-mesh using glBufferSubData
		size_t vertexOffset = 0;
		size_t indexOffset = 0;
		size_t texCoordOffset = 0;
		uint32_t baseVertex = 0;
		meshTextures.reserve(meshes.size());
		for (MVS::Mesh& submesh : meshes) {
			// Convert texture coordinates
			MVS::Mesh::TexCoordArr normFaceTexcoords;
			if (!submesh.faceTexcoords.empty()) {
				// Normalize texture coordinates
				submesh.FaceTexcoordsNormalize(normFaceTexcoords, false);
			} else {
				// Default texture coordinates
				normFaceTexcoords.resize(submesh.vertices.size());
			}

			// Convert normals to float array
			if (submesh.vertexNormals.empty())
				submesh.ComputeNormalVertices();

			// Adjust face indices to account for previous sub-meshes
			std::vector<uint32_t> adjustedIndices;
			adjustedIndices.reserve(submesh.faces.size() * 3);
			for (const MVS::Mesh::Face& face : submesh.faces) {
				adjustedIndices.push_back(baseVertex + face.x);
				adjustedIndices.push_back(baseVertex + face.y);
				adjustedIndices.push_back(baseVertex + face.z);
			}

			// Upload vertices using VBO wrapper functions
			meshVBO->SetSubData(&submesh.vertices[0].x, submesh.vertices.size() * 3, vertexOffset * 3);

			// Upload normals using VBO wrapper functions
			meshNormalVBO->SetSubData(&submesh.vertexNormals[0].x, submesh.vertexNormals.size() * 3, vertexOffset * 3);

			// Upload texture coordinates using VBO wrapper functions
			meshTexCoordVBO->SetSubData(&normFaceTexcoords[0].x, normFaceTexcoords.size() * 2, texCoordOffset * 2);

			// Upload indices using VBO wrapper functions
			meshEBO->SetSubData(adjustedIndices, indexOffset);

			// Load texture for this sub-mesh
			if (submesh.HasTexture()) {
				ASSERT(submesh.texturesDiffuse.size() == 1, "Sub-mesh should have exactly one texture");
				const MVS::IIndex i = (MVS::IIndex)meshTextures.size();
				Image& image = meshTextures.emplace_back(i);
				image.SetImageLoading();
				image.AssignImage(submesh.texturesDiffuse.front());
				// Check if image is valid and has a texture
				if (!image.TransferImage()) {
					DEBUG("Warning: Image %zu is not valid, removing from meshTextures", i);
				} else {
					image.GenerateMipmap();
				}
			}

			// Track this sub-mesh
			meshFaceCounts.emplace_back(submesh.faces.size());

			// Update offsets for next sub-mesh
			vertexOffset += submesh.vertices.size();
			texCoordOffset += submesh.vertices.size();
			indexOffset += submesh.faces.size() * 3;
			baseVertex += submesh.vertices.size();
		}
	} else {
		// Single mesh without texture - simpler case
		// Convert normals to float array
		bool hasNormals = true;
		if (mesh.vertexNormals.empty()) {
			mesh.ComputeNormalVertices();
			hasNormals = false;
		}

		// Upload to GPU using traditional method
		meshVBO->SetData(mesh.vertices[0].ptr(), mesh.vertices.size() * 3);
		meshEBO->SetData(mesh.faces[0].ptr(), mesh.faces.size() * 3);
		meshNormalVBO->SetData(mesh.vertexNormals[0].ptr(), mesh.vertexNormals.size() * 3);
		if (!hasNormals)
			mesh.vertexNormals.Release();

		meshFaceCounts.emplace_back(mesh.faces.size());
	}
}

// Helper function to compute camera frustum corners in world space
// This function correctly accounts for the principal point by using image coordinates
// and TransformPointI2W instead of assuming the principal point is at the image center
static std::array<Point3f, 4> ComputeCameraFrustumCorners(const MVS::Image& imageData, float depth) {
	// Define the 4 corners of the image in image coordinates
	// This correctly handles cases where the principal point is not at the image center
	Point3 imageCorners[4] = {
		Point3(0, 0, depth),                                    // top-left
		Point3(imageData.width, 0, depth),                      // top-right  
		Point3(imageData.width, imageData.height, depth),       // bottom-right
		Point3(0, imageData.height, depth)                      // bottom-left
	};

	// Transform corners from image space to world space
	// This automatically accounts for the principal point position
	std::array<Point3f, 4> worldCorners;
	for (int i = 0; i < 4; ++i)
		worldCorners[i] = imageData.camera.TransformPointI2W(imageCorners[i]);
	return worldCorners;
}

// Helper function to create camera frustum geometry for a single camera
// Returns the vertices, colors, and indices for the camera wireframe
static void CreateCameraFrustumGeometry(
	const MVS::Image& imageData, 
	float depth,
	const Eigen::Vector3f& centerColor,
	const Eigen::Vector3f& frustumColor,
	std::vector<float>& vertices,
	std::vector<float>& colors,
	std::vector<uint32_t>& indices,
	size_t baseIndex
) {
	// Camera center (apex of the pyramid)
	const Point3f center = imageData.camera.C;
	vertices.insert(vertices.end(), {center.x, center.y, center.z});
	colors.insert(colors.end(), {centerColor.x(), centerColor.y(), centerColor.z()});

	// Get frustum corners using the helper function
	std::array<Point3f, 4> worldCorners = ComputeCameraFrustumCorners(imageData, depth);

	// Add the 4 corners to vertices and colors
	for (int j = 0; j < 4; ++j) {
		const Point3f& worldCorner = worldCorners[j];
		vertices.insert(vertices.end(), {worldCorner.x, worldCorner.y, worldCorner.z});
		colors.insert(colors.end(), {frustumColor.x(), frustumColor.y(), frustumColor.z()});
	}

	// Add principal center point (green) - point on the image plane at the principal point
	const Point2 pp = imageData.camera.GetPrincipalPoint();
	const Point3f worldPrincipalPoint = imageData.camera.TransformPointI2W(Point3(pp.x, pp.y, depth));
	vertices.insert(vertices.end(), {worldPrincipalPoint.x, worldPrincipalPoint.y, worldPrincipalPoint.z});
	colors.insert(colors.end(), {0.f, 1.f, 0.f}); // Green

	// Add upwards direction indicator (blue) - line showing camera's up direction  
	const Point3f worldUpPoint = imageData.camera.TransformPointI2W(Point3(pp.x, pp.y - imageData.height * 0.5f, depth)); // Half way up from center
	vertices.insert(vertices.end(), {worldUpPoint.x, worldUpPoint.y, worldUpPoint.z});
	colors.insert(colors.end(), {0.f, 0.f, 1.f}); // Blue

	// Create indices for wireframe lines
	// Lines from camera center to each corner (4 lines)
	for (int j = 0; j < 4; ++j) {
		indices.push_back(baseIndex);           // camera center
		indices.push_back(baseIndex + 1 + j);   // corner j
	}

	// Rectangle connecting the four corners (4 lines)
	for (int j = 0; j < 4; ++j) {
		indices.push_back(baseIndex + 1 + j);             // current corner
		indices.push_back(baseIndex + 1 + ((j + 1) % 4)); // next corner
	}

	// Line from camera center to principal point (green indicator)
	indices.push_back(baseIndex);     // camera center
	indices.push_back(baseIndex + 5); // principal point (index 5)

	// Line from principal point to up direction point (blue indicator)
	indices.push_back(baseIndex + 5); // principal point
	indices.push_back(baseIndex + 6); // up direction point (index 6)
}

void Renderer::UploadCameras(const Window& window) {
	if (window.GetScene().GetImages().empty())
		return;

	// Generate camera frustum geometry
	const float depth = window.GetCamera().GetSceneSize().norm() * 0.01f; // Camera scale factor
	cameraIndexCount = 0;
	std::vector<float> cameraVertices;
	std::vector<float> cameraColors;
	std::vector<uint32_t> cameraIndices;

	// Use white colors for normal camera rendering
	const Eigen::Vector3f centerColor(1.f, 1.f, 1.f);   // White for center
	const Eigen::Vector3f frustumColor(1.f, 1.f, 0.f);  // Yellow for frustum

	for (const auto& image : window.GetScene().GetImages()) {
		const MVS::Image& imageData = window.GetScene().GetScene().images[image.idx];
		ASSERT(imageData.IsValid());
		// Create frustum vertices in camera coordinate system
		size_t baseIndex = cameraVertices.size() / 3;
		// Create camera frustum geometry using the shared function
		CreateCameraFrustumGeometry(
			imageData, 
			depth, 
			centerColor, 
			frustumColor,
			cameraVertices, 
			cameraColors, 
			cameraIndices, 
			baseIndex
		);
		cameraIndexCount += 20; // 10 lines * 2 indices per line
	}

	if (cameraIndexCount) {
		cameraVBO->SetData(cameraVertices);
		cameraColorVBO->SetData(cameraColors);
		cameraEBO->SetData(cameraIndices);
	}
}

void Renderer::UploadImageOverlays(const Window& window) {
	if (window.GetScene().GetImages().empty())
		return;

	// Calculate camera frustum depth (same as used in UploadCameras)
	const float depth = window.GetCamera().GetSceneSize().norm() * 0.01f;

	// Collect all overlay geometry for images with valid textures
	std::vector<float> allVertices;
	std::vector<uint32_t> allIndices;
	for (const auto& image : window.GetScene().GetImages()) {
		const MVS::Image& imageData = window.GetScene().GetScene().images[image.idx];
		ASSERT(imageData.IsValid());
		// Get frustum corners using the same helper function as UploadCameras
		std::array<Point3f, 4> worldCorners = ComputeCameraFrustumCorners(imageData, depth);
		// Create quad vertices for this overlay
		const uint32_t baseVertex = allVertices.size() / 5;
		for (int i = 0; i < 4; ++i) {
			const Point3f& worldCorner = worldCorners[i];
			// Add 3D world position
			allVertices.push_back(worldCorner.x);
			allVertices.push_back(worldCorner.y);
			allVertices.push_back(worldCorner.z);
			// Add texture coordinates
			// Map image corners to texture coordinates:
			// top-left -> (0,0), top-right -> (1,0), bottom-right -> (1,1), bottom-left -> (0,1)
			switch(i) {
				case 0: // top-left corner
					allVertices.push_back(0.f); // U
					allVertices.push_back(0.f); // V (top of image)
					break;
				case 1: // top-right corner
					allVertices.push_back(1.f); // U
					allVertices.push_back(0.f); // V (top of image)
					break;
				case 2: // bottom-right corner
					allVertices.push_back(1.f); // U
					allVertices.push_back(1.f); // V (bottom of image)
					break;
				case 3: // bottom-left corner
					allVertices.push_back(0.f); // U
					allVertices.push_back(1.f); // V (bottom of image)
					break;
			}
		}
		// Add indices for this quad (2 triangles)
		allIndices.push_back(baseVertex + 0); // top-left
		allIndices.push_back(baseVertex + 1); // top-right
		allIndices.push_back(baseVertex + 2); // bottom-right
		allIndices.push_back(baseVertex + 0); // top-left
		allIndices.push_back(baseVertex + 2); // bottom-right
		allIndices.push_back(baseVertex + 3); // bottom-left
		imageOverlayIndexCount += 6; // 2 triangles * 3 indices each
	}

	// Upload all overlay geometry to GPU buffers
	if (imageOverlayIndexCount) {
		imageOverlayVBO->Bind();
		imageOverlayVBO->SetData(allVertices);
		imageOverlayEBO->Bind();
		imageOverlayEBO->SetData(allIndices);
	}
}

void Renderer::UploadSelection(const Window& window) {
	selectionPrimitiveCount = 0;
	if (window.selectionType == Window::SEL_NA)
		return;

	// Handle point selection with valid pointViews
	std::vector<float> selectionVertices;
	const MVS::Scene& scene = window.GetScene().GetScene();
	if (window.selectionType == Window::SEL_POINT) {
		ASSERT(scene.pointcloud.IsValid() && scene.IsValid());
		// Get the selected point and its views
		const MVS::PointCloud::Point& selectedPoint = scene.pointcloud.points[window.selectionIdx];
		const MVS::PointCloud::ViewArr& pointViews = scene.pointcloud.pointViews[window.selectionIdx];
		// Create line geometry from each camera seeing this point to the point
		selectionVertices.reserve(pointViews.size() * 6); // 2 points per line, 3 coordinates per point
		for (const MVS::PointCloud::View& viewIdx : pointViews) {
			ASSERT(viewIdx < scene.images.size());
			const MVS::Image& imageData = scene.images[viewIdx];
			ASSERT(imageData.IsValid());
			// Add line from camera center to the selected point
			const Point3f& cameraCenter = imageData.camera.C;
			// First vertex: camera center
			selectionVertices.insert(selectionVertices.end(), {
				cameraCenter.x, cameraCenter.y, cameraCenter.z
			});
			// Second vertex: selected point
			selectionVertices.insert(selectionVertices.end(), {
				selectedPoint.x, selectedPoint.y, selectedPoint.z
			});
		}
	}
	// Handle triangle selection
	else if (window.selectionType == Window::SEL_TRIANGLE) {
		ASSERT(!scene.mesh.IsEmpty() && window.selectionIdx < scene.mesh.faces.size());
		const MVS::Mesh::Face& face = scene.mesh.faces[window.selectionIdx];
		const Point3f& v0 = scene.mesh.vertices[face.x];
		const Point3f& v1 = scene.mesh.vertices[face.y];
		const Point3f& v2 = scene.mesh.vertices[face.z];
		selectionVertices.reserve(18); // 3 lines * 2 vertices * 3 floats
		// Line v0-v1
		selectionVertices.insert(selectionVertices.end(), { v0.x, v0.y, v0.z });
		selectionVertices.insert(selectionVertices.end(), { v1.x, v1.y, v1.z });
		// Line v1-v2
		selectionVertices.insert(selectionVertices.end(), { v1.x, v1.y, v1.z });
		selectionVertices.insert(selectionVertices.end(), { v2.x, v2.y, v2.z });
		// Line v2-v0
		selectionVertices.insert(selectionVertices.end(), { v2.x, v2.y, v2.z });
		selectionVertices.insert(selectionVertices.end(), { v0.x, v0.y, v0.z });
	}
	// Handle camera selection
	else if (window.selectionType == Window::SEL_CAMERA) {
		const Image& image = window.GetScene().GetImages()[window.selectionIdx];
		ASSERT(!scene.images.empty() && image.idx < scene.images.size());
		const MVS::Image& selectedImage = scene.images[image.idx];
		ASSERT(selectedImage.IsValid())
		const Point3f sceneSize = window.GetCamera().GetSceneSize();
		const float depth = norm(sceneSize) * 0.01f;
		// Get frustum corners
		std::array<Point3f, 4> worldCorners = ComputeCameraFrustumCorners(selectedImage, depth);
		// Reserve space for lines: 4 (center to corners) + 4 (corner rectangle) = 8 lines × 2 vertices × 3 coordinates = 48 floats
		selectionVertices.reserve(48);
		// Lines from camera center to each corner (4 lines)
		const Point3f center = selectedImage.camera.C;
		for (int j = 0; j < 4; ++j) {
			// Line from center to corner j
			selectionVertices.insert(selectionVertices.end(), {center.x, center.y, center.z});
			selectionVertices.insert(selectionVertices.end(), {worldCorners[j].x, worldCorners[j].y, worldCorners[j].z});
		}
		// Rectangle connecting the four corners (4 lines)
		for (int j = 0; j < 4; ++j) {
			// Line from corner j to corner (j+1)%4
			const Point3f& corner1 = worldCorners[j];
			const Point3f& corner2 = worldCorners[(j + 1) % 4];
			selectionVertices.insert(selectionVertices.end(), {corner1.x, corner1.y, corner1.z});
			selectionVertices.insert(selectionVertices.end(), {corner2.x, corner2.y, corner2.z});
		}
	}

	// Set the primitive count (number of vertices)
	selectionPrimitiveCount = selectionVertices.size() / 3;

	// Add neighbor camera geometry if selected
	if (window.selectedNeighborCamera != NO_ID) {
		const Image& image = window.GetScene().GetImages()[window.selectedNeighborCamera];
		ASSERT(!scene.images.empty() && image.idx < scene.images.size());
		const MVS::Image& neighborImage = scene.images[image.idx];
		ASSERT(neighborImage.IsValid());
		const Point3f sceneSize = window.GetCamera().GetSceneSize();
		const float depth = norm(sceneSize) * 0.01f;
		// Get frustum corners for the neighbor camera
		std::array<Point3f, 4> worldCorners = ComputeCameraFrustumCorners(neighborImage, depth);
		// Lines from camera center to each corner (4 lines)
		const Point3f center = neighborImage.camera.C;
		for (int j = 0; j < 4; ++j) {
			// Line from center to corner j
			selectionVertices.insert(selectionVertices.end(), {center.x, center.y, center.z});
			selectionVertices.insert(selectionVertices.end(), {worldCorners[j].x, worldCorners[j].y, worldCorners[j].z});
		}
		// Rectangle connecting the four corners (4 lines)
		for (int j = 0; j < 4; ++j) {
			// Line from corner j to corner (j+1)%4
			const Point3f& corner1 = worldCorners[j];
			const Point3f& corner2 = worldCorners[(j + 1) % 4];
			selectionVertices.insert(selectionVertices.end(), {corner1.x, corner1.y, corner1.z});
			selectionVertices.insert(selectionVertices.end(), {corner2.x, corner2.y, corner2.z});
		}
	}

	// Upload all selection geometry to GPU if we have any
	if (!selectionVertices.empty())
		selectionVBO->SetData(selectionVertices);
}

void Renderer::UploadBounds(const MVS::Scene& scene) {
	if (!scene.IsBounded())
		return;
	Point3f::EVec corners[8];
	scene.obb.GetCorners(corners);

	// Create wireframe lines for the bounding box
	// Each line needs 2 vertices, so we'll have 12 lines * 2 vertices = 24 vertices
	boundsPrimitiveCount = 24;
	std::vector<float> wireframeVertices;
	wireframeVertices.reserve(boundsPrimitiveCount * 3);

	// Define the 12 edges of a cube by vertex indices
	// Each edge connects two corners that differ by exactly one bit (one axis)
	// Bit pattern: corner i = (bit2=z, bit1=y, bit0=x) where 0=min, 1=max
	const int edges[12][2] = {
		// X-axis edges (differ in bit 0)
		{0,1}, {2,3}, {4,5}, {6,7},
		// Y-axis edges (differ in bit 1)  
		{0,2}, {1,3}, {4,6}, {5,7},
		// Z-axis edges (differ in bit 2)
		{0,4}, {1,5}, {2,6}, {3,7}
	};

	// Generate line segments for each edge
	for (int i = 0; i < 12; ++i) {
		// First vertex of the line
		const Point3f::EVec& p1 = corners[edges[i][0]];
		wireframeVertices.push_back(p1.x());
		wireframeVertices.push_back(p1.y());
		wireframeVertices.push_back(p1.z());
		// Second vertex of the line
		const Point3f::EVec& p2 = corners[edges[i][1]];
		wireframeVertices.push_back(p2.x());
		wireframeVertices.push_back(p2.y());
		wireframeVertices.push_back(p2.z());
	}
	boundsVBO->SetData(wireframeVertices);
}

void Renderer::BeginFrame(const Camera& camera, const Eigen::Vector4f& clearColor) {
	// Set clear color and clear buffers
	GL_CHECK(glClearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w()));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

	// Update view-projection matrices
	UpdateViewProjection(camera);
}

void Renderer::UpdateViewProjection(const Camera& camera) {
	// Convert from double to float matrices
	Eigen::Matrix4d viewMatrix = camera.GetViewMatrix();
	Eigen::Matrix4d projMatrix = camera.GetProjectionMatrix();
	Eigen::Matrix4d vpMatrix = projMatrix * viewMatrix;

	ViewProjectionData vpData;
	vpData.view = viewMatrix.cast<float>();
	vpData.projection = projMatrix.cast<float>();
	vpData.viewProjection = vpMatrix.cast<float>();
	vpData.cameraPos = camera.GetPosition().cast<float>();

	viewProjectionUBO->SetData(vpData);
}

void Renderer::SetLighting(const Eigen::Vector3f& direction, float intensity, const Eigen::Vector3f& color) {
	LightingData lightData;
	lightData.lightDirection = direction.normalized();
	lightData.lightIntensity = intensity;
	lightData.lightColor = color;
	lightData.ambientStrength = 0.1f;
	lightData.ambientColor = Eigen::Vector3f(1.f, 1.f, 1.f);

	lightingUBO->SetData(lightData);
}

void Renderer::RenderPointCloud(const Window& window) {
	if (pointCount == 0) return;

	// Use the point cloud shader
	pointCloudShader->Use();

	// Set uniforms based on window settings
	pointCloudShader->SetFloat("pointSize", window.pointSize);

	pointCloudVAO->Bind();

	GL_CHECK(glDrawArrays(GL_POINTS, 0, pointCount));

	pointCloudVAO->Unbind();
}

void Renderer::RenderPointCloudNormals(const Window& window) {
	if (pointNormalCount == 0) return;

	// Use the point cloud normals shader
	pointCloudNormalsShader->Use();

	// Set normal color (cyan for good visibility)
	pointCloudNormalsShader->SetVector3("normalColor", Eigen::Vector3f(0.f, 1.f, 1.f));

	pointCloudNormalsVAO->Bind();

	GL_CHECK(glDrawArrays(GL_LINES, 0, pointNormalCount));

	pointCloudNormalsVAO->Unbind();
}

void Renderer::UpdateLighting() {
	// Implementation for updating lighting UBO if necessary, currently handled by SetLighting
}

void Renderer::RenderMesh(const Window& window) {
	if (meshFaceCounts.empty()) return;

	const bool isWireframe = window.showMeshWireframe;
	const bool texturesEnabled = window.showMeshTextured;
	if (isWireframe)
		GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
	else
		GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));

	meshVAO->Bind();
	meshEBO->Bind();

	// Render each sub-mesh
	size_t indexOffset = 0;
	FOREACH(i, meshFaceCounts) {
		const size_t faceCount = meshFaceCounts[i];

		// Check if this sub-mesh should be rendered
		if (window.meshSubMeshVisible.empty() || window.meshSubMeshVisible[i]) {
			const bool textureValid = (i < meshTextures.size()) && meshTextures[i].IsValid();

			// Check if this sub-mesh has a valid texture
			const bool hasTexture = texturesEnabled && textureValid;

			// Select the appropriate shader based on texture availability for this sub-mesh
			Shader* currentMeshShader = hasTexture ? meshTexturedShader.get() : meshShader.get();
			currentMeshShader->Use();

			// Set uniforms
			currentMeshShader->SetBool("wireframe", isWireframe);
			if (hasTexture) {
				GL_CHECK(glActiveTexture(GL_TEXTURE0));
				GL_CHECK(glBindTexture(GL_TEXTURE_2D, meshTextures[i].texture));
				currentMeshShader->SetInt("diffuseTexture", 0);
			} else {
				currentMeshShader->SetVector3("meshColor", Eigen::Vector3f(0.8f, 0.8f, 0.8f));
			}

			// Draw this sub-mesh
			const void* indexPtr = reinterpret_cast<const void*>(indexOffset * sizeof(uint32_t));
			GL_CHECK(glDrawElements(GL_TRIANGLES, faceCount * 3, GL_UNSIGNED_INT, indexPtr));
		}

		// Update offset for next sub-mesh (regardless of visibility)
		indexOffset += faceCount * 3;
	}

	meshVAO->Unbind();

	// Reset polygon mode
	GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
}

void Renderer::RenderCameras(const Window& window) {
	if (cameraIndexCount == 0)
		return;

	cameraShader->Use();

	// Get the currently selected camera from the window's camera
	MVS::IIndex selectedCamID = window.GetCamera().GetCurrentCamID();

	cameraVAO->Bind();
	cameraEBO->Bind();

	GL_CHECK(glDrawElements(GL_LINES, cameraIndexCount, GL_UNSIGNED_INT, 0));

	cameraVAO->Unbind();
}

void Renderer::RenderImageOverlays(const Window& window) {
	if (imageOverlayIndexCount == 0)
		return;

	// Save current GL state
	GLboolean depthTestEnabled;
	GL_CHECK(glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled));

	// Set up for 3D rendering with special handling for transparency
	GL_CHECK(glDisable(GL_DEPTH_TEST)); // Temporarily disable depth testing to ensure visibility
	GL_CHECK(glEnable(GL_BLEND));
	GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

	// Use the 3D overlay shader
	imageOverlayShader->Use();

	// Set opacity
	imageOverlayShader->SetFloat("opacity", window.imageOverlayOpacity);
	imageOverlayShader->SetInt("overlayTexture", 0);

	// Render the specific overlay for this camera
	imageOverlayVAO->Bind();
	imageOverlayEBO->Bind();

	FOREACH(imgIdx, window.GetScene().GetImages()) {
		// Load the image if not already loaded
		Image& image = window.GetScene().GetImages()[imgIdx];
		if (!image.IsValid()) {
			if (!image.IsImageValid())
				continue; // Image asynchronously loading
			// Image loaded, create the texture
			image.TransferImage();
		}
		// Bind the camera image texture
		GL_CHECK(glActiveTexture(GL_TEXTURE0));
		image.Bind();
		// Calculate the actual byte offset for glDrawElements
		const void* indexOffset = reinterpret_cast<const void*>(imgIdx * 6 * sizeof(uint32_t));
		GL_CHECK(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, indexOffset));
	}

	imageOverlayVAO->Unbind();

	// Restore previous depth test state
	if (depthTestEnabled)
		GL_CHECK(glEnable(GL_DEPTH_TEST));
}

void Renderer::RenderSelection(const Window& window) {
	// Only render if we have selection geometry
	if (selectionPrimitiveCount == 0)
		return;

	// Render selection lines
	selectionShader->Use();
	selectionVAO->Bind();

	// Use different colors for different selection types
	if (window.selectionType == Window::SEL_POINT) {
		selectionShader->SetVector3("selectionColor", Eigen::Vector3f(1.f, 0.f, 0.f)); // Red lines for points
	} else if (window.selectionType == Window::SEL_TRIANGLE) {
		selectionShader->SetVector3("selectionColor", Eigen::Vector3f(1.f, 0.f, 0.f)); // Red lines for triangles
	} else if (window.selectionType == Window::SEL_CAMERA) {
		selectionShader->SetVector3("selectionColor", Eigen::Vector3f(0.f, 1.f, 1.f)); // Cyan lines for cameras
	} else {
		selectionShader->SetVector3("selectionColor", Eigen::Vector3f(1.f, 1.f, 0.f)); // Yellow for other selections
	}

	// Render primary selection geometry as lines
	GL_CHECK(glDrawArrays(GL_LINES, 0, selectionPrimitiveCount));

	// Render neighbor camera with different color
	if (window.selectedNeighborCamera != NO_ID) {
		selectionShader->SetVector3("selectionColor", Eigen::Vector3f(1.f, 0.f, 1.f)); // Magenta for neighbor camera

		// Render neighbor camera geometry as lines (starting after primary selection vertices)
		GL_CHECK(glDrawArrays(GL_LINES, selectionPrimitiveCount, selectionPrimitiveCount));
	}

	selectionVAO->Unbind();
}

void Renderer::RenderBounds() {
	if (boundsPrimitiveCount == 0)
		return;

	boundsShader->Use();
	boundsShader->SetVector3("boundsColor", Eigen::Vector3f(0.f, 1.f, 0.f)); // Green

	boundsVAO->Bind();

	// Render as lines (each pair of vertices forms a line)
	GL_CHECK(glDrawArrays(GL_LINES, 0, boundsPrimitiveCount));

	boundsVAO->Unbind();
}

void Renderer::RenderCoordinateAxes(const Camera& camera) {
	if (!axesShader || !axesVAO)
		return;

	// Save current viewport and depth test state
	GLint oldViewport[4];
	GLboolean depthTestEnabled;
	GL_CHECK(glGetIntegerv(GL_VIEWPORT, oldViewport));
	GL_CHECK(glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled));

	// Set up a small viewport in the bottom right corner
	const int axesSize = 100; // Size of the axes widget
	const int margin = 10;	// Margin from screen edges

	GL_CHECK(glViewport(
		oldViewport[2] - axesSize - margin,  // x: right side minus size and margin
		margin,							     // y: bottom with margin
		axesSize,							 // width
		axesSize							 // height
	));

	// Disable depth testing
	GL_CHECK(glDisable(GL_DEPTH_TEST));

	axesShader->Use();

	// Create an orthographic projection matrix that maps [-1,1] to the widget viewport
	Eigen::Matrix4f orthoProj = Eigen::Matrix4f::Identity();
	orthoProj(0,0) = 1.5f;  // Scale X to fit nicely in widget
	orthoProj(1,1) = 1.5f;  // Scale Y to fit nicely in widget  
	orthoProj(2,2) = -0.1f; // Small Z range for orthographic

	// Get only the rotation part of the view matrix (no translation)
	Eigen::Matrix4d viewMatrix = camera.GetViewMatrix();
	Eigen::Matrix4f rotationOnlyView = Eigen::Matrix4f::Identity();
	rotationOnlyView.topLeftCorner<3, 3>() = viewMatrix.topLeftCorner<3, 3>().cast<float>();

	// Combine projection and rotation-only view
	Eigen::Matrix4f axesViewProj = orthoProj * rotationOnlyView;

	// Set the axes-specific view-projection matrix
	axesShader->SetMatrix4("viewProjection", axesViewProj);

	axesVAO->Bind();

	// Render as lines
	GL_CHECK(glDrawArrays(GL_LINES, 0, 6)); // 3 axes, 2 vertices each

	axesVAO->Unbind();

	// Restore original viewport and depth test state
	GL_CHECK(glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]));
	if (depthTestEnabled)
		GL_CHECK(glEnable(GL_DEPTH_TEST));
}

void Renderer::RenderArcballGizmos(const Camera& camera, const class ArcballControls& controls) {
	if (!gizmoShader || !gizmoVAO || !controls.getEnableGizmos())
		return;

	gizmoVAO->Bind();

	// Get the trackball center (target) and radius from the controls
	Eigen::Vector3d target = camera.GetTarget();

	// Calculate gizmo size based on camera distance and viewport
	// This mimics the three.js trackball radius calculation
	double distance = (camera.GetPosition() - target).norm();
	float gizmoRadius;

	if (camera.IsOrthographic()) {
		// For orthographic camera, use a fixed size relative to viewport
		float minSide = MINF(camera.GetSize().width, camera.GetSize().height);
		gizmoRadius = minSide * 0.67f / (2.f * 1.f); // Assume zoom = 1.0 for now
	} else {
		// For perspective camera, calculate based on FOV and distance
		float fov = D2R(camera.GetFOV());
		float minSide = MINF(camera.GetSize().width, camera.GetSize().height);
		gizmoRadius = distance * TAN(fov / 2.f) * 0.67f * minSide / camera.GetSize().height;
	}

	// Set transparency based on active state
	float opacity = controls.getGizmosActive() ? 1.f : 0.6f;

	// Colors for X, Y, Z axes (red, green, blue)
	Eigen::Vector3f colors[3] = {
		Eigen::Vector3f(1.f, 0.5f, 0.5f), // X - red
		Eigen::Vector3f(0.5f, 1.f, 0.5f), // Y - green  
		Eigen::Vector3f(0.5f, 0.5f, 1.f)  // Z - blue
	};

	// Render three circles for X, Y, Z axes using the gizmo shader
	gizmoShader->Use();

	for (int axis = 0; axis < 3; ++axis) {
		// Create transformation matrix for each circle
		Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();

		// Translate to target position
		transform.col(3).head<3>() = target.cast<float>();

		// Scale to gizmo radius
		transform.topLeftCorner<3, 3>() *= gizmoRadius;

		// Rotate circle to align with axis
		if (axis == 0) {
			// X-axis: rotate 90 degrees around Y-axis
			transform.topLeftCorner<3, 3>() *= Eigen::AngleAxisf(FHALF_PI, Eigen::Vector3f::UnitY()).toRotationMatrix();
		} else if (axis == 2) {
			// Z-axis: rotate 90 degrees around X-axis  
			transform.topLeftCorner<3, 3>() *= Eigen::AngleAxisf(FHALF_PI, Eigen::Vector3f::UnitX()).toRotationMatrix();
		}
		// Y-axis uses default circle orientation (no additional rotation needed)

		// Set uniforms
		gizmoShader->SetMatrix4("modelMatrix", transform);
		gizmoShader->SetVector3("gizmoColor", colors[axis]);
		gizmoShader->SetFloat("opacity", opacity);

		// Render circle as lines
		GL_CHECK(glDrawElements(GL_LINES, gizmoCircleIndexCount, GL_UNSIGNED_INT, 0));
	}

	// Render gizmo center axes if enabled
	if (controls.getEnableGizmosCenter()) {
		// Continue using the same gizmo shader for consistency
		// Render each axis with its corresponding color
		for (int axis = 0; axis < 3; ++axis) {
			// Create transformation matrix for the center axes
			Eigen::Matrix4f centerTransform = Eigen::Matrix4f::Identity();

			// Translate to target position
			centerTransform.col(3).head<3>() = target.cast<float>();

			// Scale to a smaller size (relative to gizmo radius)
			float centerScale = gizmoRadius * 0.15f; // 15% of gizmo radius
			centerTransform.topLeftCorner<3, 3>() *= centerScale;

			// Set uniforms
			gizmoShader->SetMatrix4("modelMatrix", centerTransform);
			gizmoShader->SetVector3("gizmoColor", colors[axis]); // Use same colors as circles
			gizmoShader->SetFloat("opacity", opacity);

			// Calculate vertex range for this axis (2 vertices per axis)
			int axisBaseVertex = gizmoCenterAxesBaseVertex + (axis * 2);

			// Render this axis as lines
			GL_CHECK(glDrawArrays(GL_LINES, axisBaseVertex, 2));
		}
	}

	gizmoVAO->Unbind();
}

void Renderer::RenderSelectionOverlay(const Window& window) {
	// Only render overlay if in selection mode  
	if (window.GetControlMode() != Window::CONTROL_SELECTION)
		return;
	SelectionController& selectionController = window.GetSelectionController();
	// Only render if selecting or has a selection
	if (!selectionController.isSelecting() && !selectionController.hasSelection())
		return;
	// Safety check: ensure all required objects are initialized
	if (!selectionOverlayShader || !selectionOverlayVAO || !selectionOverlayVBO)
		return;
	// Disable depth testing for 2D overlay
	GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
	if (depthTestEnabled)
		GL_CHECK(glDisable(GL_DEPTH_TEST));

	// Set up 2D rendering state
	GL_CHECK(glDisable(GL_CULL_FACE));

	selectionOverlayShader->Use();
	selectionOverlayShader->SetVector3("overlayColor", Eigen::Vector3f(1.f, 1.f, 0.f)); // Yellow
	selectionOverlayShader->SetFloat("overlayOpacity", 0.8f);

	selectionOverlayVAO->Bind();

	if (selectionController.getSelectionMode() == SelectionController::MODE_BOX) {
		// Render box selection if active
		if (selectionController.isSelecting()) {
			const auto& start = selectionController.getSelectionStart();
			const auto& end = selectionController.getSelectionEnd();
			// SelectionController coordinates are already normalized [-1, 1]
			float x1 = static_cast<float>(start.x());
			float y1 = static_cast<float>(start.y());
			float x2 = static_cast<float>(end.x());
			float y2 = static_cast<float>(end.y());
			std::vector<float> boxVertices = {
				x1, y1,
				x2, y1,
				x2, y2,
				x1, y2,
				x1, y1
			};
			selectionOverlayVBO->SetData(boxVertices);
			GL_CHECK(glDrawArrays(GL_LINE_STRIP, 0, 5));
		}
	} else {
		// Render lasso/circle selection path
		const auto& path = selectionController.getCurrentSelectionPath();
		if (!path.empty()) {
			std::vector<float> pathVertices;
			pathVertices.reserve(path.size() * 2);
			for (const auto& point : path) {
				// SelectionController coordinates are already normalized [-1, 1]
				float x = static_cast<float>(point.x());
				float y = static_cast<float>(point.y());
				pathVertices.push_back(x);
				pathVertices.push_back(y);
			}
			if (!pathVertices.empty()) {
				selectionOverlayVBO->SetData(pathVertices);
				GL_CHECK(glDrawArrays(GL_LINE_STRIP, 0, pathVertices.size() / 2));
			}
		}
	}

	selectionOverlayVAO->Unbind();

	// Restore OpenGL state
	if (depthTestEnabled)
		GL_CHECK(glEnable(GL_DEPTH_TEST));
}

void Renderer::RenderSelectedGeometry(const Window& window) {
	// Render selected geometry regardless of control mode, as long as we have selections
	const SelectionController& selectionController = window.GetSelectionController();
	if (!selectionController.hasSelection())
		return;

	// Enable blending for highlighting effect
	glIsEnabled(GL_DEPTH_TEST);
	GL_CHECK(glEnable(GL_BLEND));
	GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

	// Use the geometry selection shader for highlighting
	geometrySelectionShader->Use();
	geometrySelectionShader->SetBool("useHighlight", true);
	geometrySelectionShader->SetFloat("highlightOpacity", 0.8f);

	// Render selected points with highlighting
	const auto& selectedPointIndices = selectionController.getSelectedPointIndices();
	if (window.showPointCloud && !selectedPointIndices.empty() && pointCount > 0) {
		// Set highlight color for points (bright red)
		geometrySelectionShader->SetVector3("highlightColor", Eigen::Vector3f(1.f, 0.1f, 0.1f));
		geometrySelectionShader->SetFloat("pointSize", window.pointSize * 2.5f);

		// Create a temporary VBO with only selected point data
		std::vector<float> selectedPoints;
		selectedPoints.reserve(selectedPointIndices.size() * 3);

		// We need access to the actual point cloud data to extract selected points
		// For now, we'll use a different approach - render individual points
		pointCloudVAO->Bind();
		GL_CHECK(glEnable(GL_PROGRAM_POINT_SIZE));

		// Render each selected point individually using glDrawArrays with offset
		for (const auto& pointIdx : selectedPointIndices) {
			if (pointIdx < pointCount)
				GL_CHECK(glDrawArrays(GL_POINTS, pointIdx, 1));
		}

		pointCloudVAO->Unbind();
	}

	// Render selected faces with highlighting (wireframe overlay)
	const auto& selectedFaceIndices = selectionController.getSelectedFaceIndices();
	if (window.showMesh && !selectedFaceIndices.empty() && !meshFaceCounts.empty()) {
		// Set highlight color for faces (bright orange/yellow)
		geometrySelectionShader->SetVector3("highlightColor", Eigen::Vector3f(1.f, 0.f, 0.f));

		// Render as wireframe overlay to show selection
		GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));

		// Enable polygon offset to render selection on top of existing mesh
		GL_CHECK(glEnable(GL_POLYGON_OFFSET_LINE));
		GL_CHECK(glPolygonOffset(-1.f, -1.f)); // More aggressive offset

		meshVAO->Bind();
		meshEBO->Bind();

		// Render only selected faces individually
		// Each face consists of 3 vertices (triangle)
		for (const auto& faceIdx : selectedFaceIndices) {
			// Get which submesh this face belongs to and its index within that submesh
			const MVS::Scene& scene = window.GetScene().GetScene();
			ASSERT(scene.mesh.faceTexindices.empty() || scene.mesh.faceTexindices.size() == scene.mesh.faces.size());
			const MVS::Mesh::TexIndex submeshIdx = scene.mesh.GetFaceTextureIndex(faceIdx);
			ASSERT(submeshIdx < meshFaceCounts.size());
			// Check if this submesh is visible
			if (!window.meshSubMeshVisible.empty() && !window.meshSubMeshVisible[submeshIdx])
				continue;
			// Calculate the actual byte offset for this face in the EBO
			MVS::Mesh::FIndex faceIdxInSubmesh(mapFaceSubsetIndices.empty() ? faceIdx :  mapFaceSubsetIndices[faceIdx]);
			size_t submeshOffset = 0;
			for (MVS::Mesh::TexIndex i = 0; i < submeshIdx; ++i)
				submeshOffset += meshFaceCounts[i];
			const void* indexPtr = reinterpret_cast<const void*>((submeshOffset + faceIdxInSubmesh) * 3 * sizeof(uint32_t));
			// Render this single face (3 indices)
			GL_CHECK(glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indexPtr));
		}

		meshVAO->Unbind();

		// Restore rendering state
		GL_CHECK(glDisable(GL_POLYGON_OFFSET_LINE));
		GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
	}

	// Reset shader state
	geometrySelectionShader->SetBool("useHighlight", false);

	// Restore OpenGL state
	GL_CHECK(glDisable(GL_BLEND));
}

void Renderer::EndFrame() {
	// Swap buffers is handled by GLFW in the Window class
	// This method can be used for cleanup or final operations if needed
}
/*----------------------------------------------------------------*/
