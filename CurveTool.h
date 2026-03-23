#pragma once
#include "Scene/SceneDocument.h"

#include <tge/math/BezierCurve.h>
#include <tge/math/Matrix4x4.h>
#include <tge/model/AnimatedModelInstance.h>
#include <tge/model/ModelInstance.h>
#include <tge/model/PaintableModelInstance.h>
#include <tge/stringRegistry/StringRegistry.h>

namespace Tga
{
	class SceneObject;
	class Scene;

	struct SceneVertexPaintableModel;
	struct SceneModel;
	class ViewportInterface;
	class Camera;
}

namespace Forge
{
	struct PointSnap
	{
		bool snapPos = false;
		bool snapRot = false;
		bool snapScale = false;

		float pos = 100.f;
		float rot = 45.f;
		float scale = 0.1f;
	};

	enum class Alignment
	{
		Right,
		Up,
		Forward,
		Invalid,
		count
	};

	class CurveTool
	{
	public:
		CurveTool();
		const BezierCurve& GetCurve() const;
		BezierCurve& AccessCurve();

		bool IsToolActive() const { return myToolActive; }
		bool IsCurveRendered() const { return myToolActive && myRenderCurve; }
		bool IsGizmosActive() const { return myToolActive && myEditCurve && myRenderCurve && myShowGizmos; }

		void SetToolActive(bool aIsActive) { myToolActive = aIsActive; }
		void HandleInputAndRenderUI();
		void DrawCurve();

		void ToolSummarization();
		void DrawGizmos(const Tga::Camera& camera, Tga::ViewportInterface& aViewportInterface, Tga::Vector2i aViewportPos, Tga::Vector2i aViewportSize);
		void ClearUndo();

	private:
		void PlacementUI();

		void NameObject(Tga::Scene* scene, std::shared_ptr<Tga::SceneObject> object, Tga::StringId& objectDefinitionName);
		void TransformObject(std::shared_ptr<Tga::SceneObject> object, const Tga::Vector3f& point);
		void RandomizeTRS(Tga::TRS& aTrs);
		void AddObjectToScene(Tga::Scene* scene, std::shared_ptr<Tga::SceneObject> object, Tga::SceneDocument* sceneDocument);
		void PlaceObjectsAlongCurve(Tga::StringId& objectDefinitionName);
		void PlaceObjectsAlongCurve(std::vector<Tga::StringId>& objectDefinitionNames);
		Tga::Matrix4x4f GetAlignedTransform(float nextPercentage, const Tga::Vector3f& posOfCurrentObject);
		Tga::Matrix4x4f GetAlignedTransformFromLast(const Tga::Vector3f& newForward, const Tga::Vector3f& posOfCurrentObject);
		void AlignObjectsAlongCurve(Tga::StringId& objectDefinitionName);
		void AutoAlignObjects(Tga::StringId& objectDefinitionName);
		Tga::ModelInstance ReadModelInstance(const Tga::SceneModel& aSceneModel);
		Tga::AnimatedModelInstance ReadAnimatedModelInstance(const Tga::SceneModel& aSceneModel);
		Tga::PaintableModelInstance ReadPaintableModelInstance(const Tga::SceneVertexPaintableModel&);
		void HelpMarker(const char* aDescription, bool aSameLine = true);
		void MoveCurveInFrontOfCamera();
		void EditPoints();
		void Debug();
		Tga::Vector3f GetPositionInFrontOfCamera();
		void UpdateTranslation(uint8_t aCurvePointIndex, const Tga::Vector3f& referencePosition, const Tga::Matrix4x4f& transform);


		BezierCurve myCurve;
		std::vector<uint32_t> myIdOfRecentlyAddedObjects;
		std::vector<Tga::TRS> myRandomOffsetPreviews;
		Tga::Matrix4x4f myManipulationCurrentTransform;
		Tga::Matrix4x4f myManipulationInitialTransform;
		Tga::Vector3f myManipulationStartPos;
		PointSnap mySnap;

		uint16_t myCurrentOperation = 7;
		uint16_t myCurrentMode = 0;
		uint8_t myPointToEditIndex = 0;

		Alignment myAlignment = Alignment::Invalid;

		float myAheadOfCamera = 400.f;
		float myHorizontalOffsetLength = 200.f;
		float myVerticalOffsetLength = 200.f;
		float mySpacing = 0.f;

		int myRandomXRotation = 0;
		int myRandomYRotation = 0;
		int myRandomZRotation = 0;

		int myNumberOfObjectsToPlace = 0;

		bool myShowGizmos = true;
		bool myIsManipulating = false;
		bool myToolActive = false;
		bool myRenderCurve = false;
		bool myRenderCameraAim = false;
		bool myEditCurve = true;
		bool myCurveDebug = false;
		bool myPlaceObjectsAlongCurve = false;
		bool myWatchPlacementPreview = false;
		bool myRenderGizmos = true;
		bool myHasRandomRotation = false;
		bool myHasRandomHorizontalOffset = false;
		bool myHasRandomVerticalOffset = false;
		bool myPlaceObjectsOfDifferentTypes = false;
		bool myPlaceDifferentTypesInRandomOrder = false;

	};

}
