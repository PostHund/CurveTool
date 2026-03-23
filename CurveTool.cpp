#include "CurveTool.h"

#include "Editor.h"
// #include "../../../Game/source/Utilities/ScenePropertyExtractor.h"
#include "Commands/AddSceneObjectsCommand.h"
#include "imgui_widgets/DragDropCommon.h"
#include "Scene/ActiveScene.h"
#include "Scene/SceneDocument.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <IconFontHeaders/IconsLucide.h>
#include <magic_enum/magic_enum.hpp>
#include <tge/engine.h>
#include <tge/drawers/DebugDrawer.h>
#include <tge/ImGui/ImGuiInterface.h>
#include <tge/primitives/LinePrimitive.h>
#include <tge/scene/Scene.h>
#include <tge/scene/SceneObjectDefinition.h>
#include <tge/scene/ScenePropertyExtractor.h>
#include <tge/scene/ScenePropertyTypes.h>
#include <tge/settings/settings.h>
#include <tge/texture/TextureManager.h>

namespace Forge
{
	struct RenderModelInstance;
}

Forge::CurveTool::CurveTool()
{
	std::vector<Tga::Vector3f> points(4);
	points[0] = { 0.f, 0.f, 0.f };
	points[1] = { 0.f, 500.f, 0.f };
	points[2] = { 500.f, 500.f, 0.f };
	points[3] = { 500.f, 0.f, 0.f };

	myCurve.SetCurvePoints(points);
	myCurrentOperation = ImGuizmo::TRANSLATE;
	myCurrentMode = ImGuizmo::LOCAL;
}

const Forge::BezierCurve& Forge::CurveTool::GetCurve() const
{
	return myCurve;
}

Forge::BezierCurve& Forge::CurveTool::AccessCurve()
{
	return myCurve;
}

void Forge::CurveTool::HandleInputAndRenderUI()
{
	if (!myToolActive)
	{
		return;
	}

	ImGui::SetNextWindowClass(Tga::Editor::GetEditor()->GetGlobalWindowClass());
	{
		ImGui::Begin("Curve Tool Settings", &myToolActive);

		//Edit curve

		ImGui::Separator();
		ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
		ImGui::Checkbox(ICON_LC_SPLINE, &myEditCurve);
		ImGui::PopFont();
		ImGui::SetItemTooltip("Edit Curve");
		if (myEditCurve)
		{
			myRenderCurve = true;
			myRenderGizmos = true;
			EditPoints();
		}

		//Place objects along curve
		ImGui::Separator();
		ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
		ImGui::Checkbox(ICON_LC_TREE_PINE, &myPlaceObjectsAlongCurve);
		ImGui::PopFont();
		ImGui::SetItemTooltip("Place objects along curve");

		if (myPlaceObjectsAlongCurve)
		{
			PlacementUI();
		}

		ImGui::Separator();
		ImGui::Checkbox("Other Debug Options", &myCurveDebug);
		if (myCurveDebug)
		{
			Debug();
		}

		ImGui::End();
	}
}

void Forge::CurveTool::DrawCurve()
{
	Tga::DebugDrawer& debugDrawer = Tga::Engine::GetInstance()->GetDebugDrawer();
	myCurve.DrawBezierCurve();
	myCurve.DrawBasePoints(30.f);
	if (myEditCurve)
	{
		Tga::LinePrimitive line;
		line.color = myCurve.GetColorA() - Tga::Vector4f{ 0.2f, 0.2f, 0.2f, 0.2f };
		line.fromPosition = myCurve.GetPointOnCurve(0.25f);
		line.toPosition = myCurve[1];
		debugDrawer.DrawPrimitiveLine(line);

		line.color = myCurve.GetColorB() - Tga::Vector4f{ 0.2f, 0.2f, 0.2f, 0.2f };
		line.fromPosition = myCurve[2];
		line.toPosition = myCurve.GetPointOnCurve(0.75f);
		debugDrawer.DrawPrimitiveLine(line);
	}

	if (myRenderCameraAim)
	{
		Tga::SceneDocument* scene = Tga::Editor::GetEditor()->GetActiveSceneDocument();
		if (scene != nullptr)
		{
			const Tga::Vector3f cameraForward = scene->GetViewport().GetCamera().GetTransform().GetForward();
			const Tga::Vector3f cameraPos = scene->GetViewport().GetCamera().GetTransform().GetPosition();
			const Tga::Vector3f aimPos = cameraPos + cameraForward * myAheadOfCamera;
			debugDrawer.DrawSphere(aimPos, 40.f, { 1.f, 0.f, 0.f, 1.f });
		}
	}

	if (myWatchPlacementPreview)
	{
		// calculate all points on curve
		for (int i = 0; i < myNumberOfObjectsToPlace; ++i)
		{
			const float percentage = static_cast<float>(i) / static_cast<float>(myNumberOfObjectsToPlace - 1);
			Tga::Vector3f point = myCurve.GetPointOnCurve(percentage);

			if (!myRandomOffsetPreviews.empty())
			{
				point += myRandomOffsetPreviews[i].translation;
			}

			debugDrawer.DrawSphere(point, 50.f, { 1.f, 0.f, 0.f, 1.f });
		}
	}
}

void Forge::CurveTool::ToolSummarization()
{
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip())
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted("This is primarily a level design tool\n"
			"It can be used to quickly place out terrain in different procedural patterns\n"
			"Get creative! If you have questions ask Ivar");
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void Forge::CurveTool::PlacementUI()
{
	//TODO: comment this section to make it more readable.

	static Tga::StringId objectDefinitionName0 = Tga::StringRegistry::RegisterOrGetString("<Object type ...>");
	static const Tga::StringId resetName = Tga::StringRegistry::RegisterOrGetString("<Object type ...>");
	static bool placeNow = false;

	placeNow = false;
	if (ImGui::Button("Reset object data"))
	{
		objectDefinitionName0 = resetName;
		myNumberOfObjectsToPlace = 0;
		myHasRandomRotation = false;
		placeNow = false;
	}

	// drag and drop object(s) to places
	ImGui::Text(objectDefinitionName0.GetString());
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAG_PAYLOAD_TYPE_STR[static_cast<int>(DragPayloadType::TGO)]))
		{
			const std::string_view payloadView = static_cast<const char*>(payload->Data);
			objectDefinitionName0 = Tga::StringRegistry::RegisterOrGetString(fs::path(payloadView).stem().string());
		}
		ImGui::EndDragDropTarget();
	}
	ImGui::Checkbox("Place more than one type", &myPlaceObjectsOfDifferentTypes);
	static std::vector<Tga::StringId> extraObjectDefinitionsNames;
	static int numberOfTypes = 0;
	ImGui::SameLine();
	if (ImGui::Button("Reset multi-objects"))
	{
		myPlaceObjectsOfDifferentTypes = false;
		extraObjectDefinitionsNames.clear();
	}
	if (myPlaceObjectsOfDifferentTypes)
	{
		ImGui::SameLine();
		ImGui::Checkbox("Place in random order", &myPlaceDifferentTypesInRandomOrder);
		ImGui::SameLine();
		ImGui::PushItemWidth(100.f);
		if (ImGui::DragInt("Number of types", &numberOfTypes, 1, 2, 10))
		{
			extraObjectDefinitionsNames.clear();
			extraObjectDefinitionsNames.resize(numberOfTypes, resetName);
		}
		ImGui::PopItemWidth();
		if (!extraObjectDefinitionsNames.empty())
		{
			ImGui::Text(extraObjectDefinitionsNames[0].GetString());
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAG_PAYLOAD_TYPE_STR[static_cast<int>(DragPayloadType::TGO)]))
				{
					const std::string_view payloadView = static_cast<const char*>(payload->Data);
					objectDefinitionName0 = Tga::StringRegistry::RegisterOrGetString(fs::path(payloadView).stem().string());
					extraObjectDefinitionsNames[0] = objectDefinitionName0;
				}
				ImGui::EndDragDropTarget();
			}
		}
		for (int defIndex = 1; defIndex < extraObjectDefinitionsNames.size(); ++defIndex)
		{
			ImGui::Text(extraObjectDefinitionsNames[defIndex].GetString());
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAG_PAYLOAD_TYPE_STR[static_cast<int>(DragPayloadType::TGO)]))
				{
					const std::string_view payloadView = static_cast<const char*>(payload->Data);
					extraObjectDefinitionsNames[defIndex] = Tga::StringRegistry::RegisterOrGetString(fs::path(payloadView).stem().string());
				}
				ImGui::EndDragDropTarget();
			}
		}
	}

	ImGui::PushItemWidth(100.f);
	ImGui::DragInt("Number of objects along curve", &myNumberOfObjectsToPlace, 1, 0, 100);
	ImGui::PopItemWidth();

	auto setRandomPreviewOffsets = [this]()
		{
			myRandomOffsetPreviews.clear();
			myRandomOffsetPreviews.resize(myNumberOfObjectsToPlace);

			for (Tga::TRS& trs : myRandomOffsetPreviews)
			{
				RandomizeTRS(trs);
			}
		};

	if (ImGui::Checkbox("Place with random rotation", &myHasRandomRotation))
	{
		setRandomPreviewOffsets();
	}
	if (myHasRandomRotation)
	{
		ImGui::PushItemWidth(50.f);
		ImGui::SameLine();
		if (ImGui::DragInt("X", &myRandomXRotation, 1, 0, 359))
		{
			setRandomPreviewOffsets();
		}
		ImGui::SameLine();
		if (ImGui::DragInt("Y", &myRandomYRotation, 1, 0, 359))
		{
			setRandomPreviewOffsets();
		}
		ImGui::SameLine();
		if (ImGui::DragInt("Z", &myRandomZRotation, 1, 0, 359))
		{
			setRandomPreviewOffsets();
		}
		ImGui::PopItemWidth();
	}
	if (ImGui::Checkbox("Random Horizontal Offset", &myHasRandomHorizontalOffset))
	{
		setRandomPreviewOffsets();
	}
	if (myHasRandomHorizontalOffset)
	{
		ImGui::PushItemWidth(50.f);
		ImGui::SameLine();
		if (ImGui::DragFloat("Horizontal Offset length", &myHorizontalOffsetLength, 1.f, 0, 100000.f, "%1.f"))
		{
			setRandomPreviewOffsets();
		}
		ImGui::PopItemWidth();
	}
	if (ImGui::Checkbox("Random Vertical Offset", &myHasRandomVerticalOffset))
	{
		setRandomPreviewOffsets();
	}
	if (myHasRandomVerticalOffset)
	{
		ImGui::PushItemWidth(50.f);
		ImGui::SameLine();
		if (ImGui::DragFloat("Vertical Offset length", &myVerticalOffsetLength, 1.f, 0, 100000.f, "%1.f"))
		{
			setRandomPreviewOffsets();
		}
		ImGui::PopItemWidth();
	}

	// placement do, preview och undo
	if (objectDefinitionName0 == resetName || myNumberOfObjectsToPlace < 1)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.6f, 0.6f)));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.7f, 0.7f)));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.8f, 0.8f)));
		ImGui::Button("Not ready to place");
		ImGui::PopStyleColor(3);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::HSV(3.f / 7.0f, 0.6f, 0.6f)));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, static_cast<ImVec4>(ImColor::HSV(3.f / 7.0f, 0.7f, 0.7f)));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, static_cast<ImVec4>(ImColor::HSV(3.f / 7.0f, 0.8f, 0.8f)));
		if (ImGui::Button("   Ready to place   "))
		{
			placeNow = true;
		}
		ImGui::PopStyleColor(3);
	}
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::HSV(1.f / 7.0f, 0.6f, 0.6f)));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, static_cast<ImVec4>(ImColor::HSV(1.f / 7.0f, 0.7f, 0.7f)));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, static_cast<ImVec4>(ImColor::HSV(1.f / 7.0f, 0.8f, 0.8f)));
	ImGui::Button("Preview placement");

	if (ImGui::IsItemActive())
	{
		myWatchPlacementPreview = true;
		myWatchPlacementPreview = myNumberOfObjectsToPlace > 0;
	}
	else
	{
		myWatchPlacementPreview = false;
	}
	ImGui::PopStyleColor(3);

	if (placeNow)
	{
		placeNow = false;
		myIdOfRecentlyAddedObjects.clear();
		if (objectDefinitionName0 != resetName && !myPlaceObjectsOfDifferentTypes)
		{
			PlaceObjectsAlongCurve(objectDefinitionName0);
		}
		else if (!extraObjectDefinitionsNames.empty() && myPlaceObjectsOfDifferentTypes)
		{
			bool allNamesOk = true;

			for (auto& nameOk : extraObjectDefinitionsNames)
			{
				if (nameOk == resetName)
				{
					allNamesOk = false;
					break;
				}
			}
			if (allNamesOk)
			{
				PlaceObjectsAlongCurve(extraObjectDefinitionsNames);
			}
		}
	}
	const bool hasRecentPlacement = !myIdOfRecentlyAddedObjects.empty();
	if (hasRecentPlacement)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.6f, 0.6f)));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.7f, 0.7f)));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.8f, 0.8f)));
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::HSV(6.f / 7.0f, 0.3f, 0.3f)));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, static_cast<ImVec4>(ImColor::HSV(6.f / 7.0f, 0.6f, 0.6f)));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, static_cast<ImVec4>(ImColor::HSV(6.f / 7.0f, 0.7f, 0.7f)));
	}
	ImGui::SameLine();
	auto undo = [this, hasRecentPlacement]()
		{
			if (!hasRecentPlacement)
			{
				return;
			}
			Tga::SceneDocument* sceneDocument = Tga::Editor::GetEditor()->GetActiveSceneDocument();
			Tga::Scene* scene = nullptr;
			if (sceneDocument)
			{
				scene = sceneDocument->AccessScene();
			}
			if (scene)
			{
				Tga::SetActiveScene(scene);
				// Tga::SceneSelection::GetActiveSceneSelection()->ClearSelection();
				for (uint32_t id : myIdOfRecentlyAddedObjects)
				{
					sceneDocument->AccessSceneSelection().RemoveFromSelection(id);
					scene->DeleteSceneObject(id);
				}
				myIdOfRecentlyAddedObjects.clear();
				sceneDocument->SetSceneDirty();
				Tga::SetActiveScene(nullptr);
			}

		};
	if (ImGui::Button("Undo recent placement"))
	{
		undo();
	}
	ImGui::PopStyleColor(3);

	static bool align = false;

	ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
	ImGui::Checkbox(ICON_LC_TRAIN_TRACK, &align);
	ImGui::PopFont();
	ImGui::SetItemTooltip("Place objects along curve, axially aligned");

	if (align)
	{
		bool okToAlign = (myNumberOfObjectsToPlace > 0) && objectDefinitionName0 != resetName && myAlignment != Alignment::Invalid;

		if (!okToAlign)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.6f, 0.6f)));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.7f, 0.7f)));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.8f, 0.8f)));
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::HSV(3.f / 7.0f, 0.6f, 0.6f)));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, static_cast<ImVec4>(ImColor::HSV(3.f / 7.0f, 0.7f, 0.7f)));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, static_cast<ImVec4>(ImColor::HSV(3.f / 7.0f, 0.8f, 0.8f)));
		}
		if (ImGui::Button("Place objects with axis following curve") && okToAlign)
		{
			AlignObjectsAlongCurve(objectDefinitionName0);
		}
		ImGui::PopStyleColor(3);
		ImGui::SameLine();
		Alignment value = myAlignment;
		int item_current_idx = static_cast<int>(value);

		ImGui::PushItemWidth(100.f);
		if (ImGui::BeginCombo("Select axis to align", magic_enum::enum_name(myAlignment).data()))
		{
			for (int alignmentIndex = 0; alignmentIndex < static_cast<int>(Alignment::count); ++alignmentIndex)
			{
				const bool is_selected = (item_current_idx == alignmentIndex);
				Alignment label = static_cast<Alignment>(alignmentIndex);
				auto labelName = magic_enum::enum_name(label);

				if (ImGui::Selectable(std::string(labelName).c_str(), is_selected))
				{
					item_current_idx = alignmentIndex;
					Alignment newValue = static_cast<Alignment>(alignmentIndex);
					myAlignment = newValue;
				}
			}
			ImGui::EndCombo();
		}

		bool okToAutoAlign = objectDefinitionName0 != resetName && myAlignment != Alignment::Invalid;
		if (!okToAutoAlign)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.6f, 0.6f)));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.7f, 0.7f)));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, static_cast<ImVec4>(ImColor::HSV(0.f / 7.0f, 0.8f, 0.8f)));
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::HSV(3.f / 7.0f, 0.6f, 0.6f)));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, static_cast<ImVec4>(ImColor::HSV(3.f / 7.0f, 0.7f, 0.7f)));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, static_cast<ImVec4>(ImColor::HSV(3.f / 7.0f, 0.8f, 0.8f)));
		}

		ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
		const char* botIcon = okToAutoAlign ? ICON_LC_BOT : ICON_LC_BOT_OFF;
		if (ImGui::Button(botIcon) && okToAutoAlign)
		{
			AutoAlignObjects(objectDefinitionName0);
		}
		ImGui::PopFont();
		ImGui::SetItemTooltip("Let the tool calculate the appropriate amount of objects to place");
		ImGui::PopStyleColor(3);
		ImGui::SameLine();
		ImGui::PushItemWidth(100.f);
		ImGui::DragFloat("Spacing when autoaligning", &mySpacing, 1.f, -2000.f, 2000.f, "%.1f");
		ImGui::PopItemWidth();


	}
}

void Forge::CurveTool::NameObject(Tga::Scene* scene, std::shared_ptr<Tga::SceneObject> object, Tga::StringId& objectDefinitionName)
{
	object->SetSceneObjectDefinitionName(objectDefinitionName);

	if (scene->GetFirstSceneObject(objectDefinitionName.GetString()) == nullptr)
	{
		object->SetName(objectDefinitionName.GetString());
	}
	else
	{
		char buffer[512];

		int i = 1;

		while (true)
		{
			sprintf_s(buffer, "%s(%i)", objectDefinitionName.GetString(), i);
			if (scene->GetFirstSceneObject(buffer) == nullptr)
			{
				object->SetName(buffer);
				break;
			}
			i++;
		}
	}
}

void Forge::CurveTool::TransformObject(std::shared_ptr<Tga::SceneObject> object, const Tga::Vector3f& point)
{
	object->GetTRS().translation = point;
	RandomizeTRS(object->GetTRS());
}

void Forge::CurveTool::RandomizeTRS(Tga::TRS& aTrs)
{
	if (myHasRandomHorizontalOffset)
	{
		int degrees = rand() % 360;

		float radians = (FMath::Pi / 180) * degrees;

		float x = cosf(radians);
		float z = sinf(radians);

		Tga::Vector3f direction = { x, 0.f, z };

		aTrs.translation += direction * myHorizontalOffsetLength;
	}
	if (myHasRandomVerticalOffset)
	{
		int degrees = rand() % 360;

		float radians = (FMath::Pi / 180) * degrees;

		float dir = cosf(radians);

		Tga::Vector3f direction = { 0.f, dir, 0.f };

		aTrs.translation += direction * myVerticalOffsetLength;
	}
	if (myHasRandomRotation)
	{
		const Tga::Vector3<int> randRot = {
			rand() % (myRandomXRotation + 1),
			rand() % (myRandomYRotation + 1),
			rand() % (myRandomZRotation + 1)
		};
		aTrs.rotation = {
			static_cast<float>(randRot.x),
			static_cast<float>(randRot.y),
			static_cast<float>(randRot.z)
		};
	}
}

void Forge::CurveTool::AddObjectToScene(Tga::Scene* scene, std::shared_ptr<Tga::SceneObject> object, Tga::SceneDocument* sceneDocument)
{

	std::shared_ptr<Tga::AddSceneObjectsCommand> command = std::make_shared<Tga::AddSceneObjectsCommand>();
	command->AddObjects(std::span<std::shared_ptr<Tga::SceneObject>>(&object, 1));
	Tga::SetActiveScene(scene);
	Tga::CommandManager::DoCommand(command);

	Tga::SceneSelection* sceneSelection = Tga::SceneSelection::GetActiveSceneSelection();
	std::span<const std::pair<uint32_t, std::shared_ptr<Tga::SceneObject>>> createdObjects = command->GetObjects();

	for (const std::pair<uint32_t, std::shared_ptr<Tga::SceneObject>>& p : createdObjects)
	{
		myIdOfRecentlyAddedObjects.push_back(p.first);
	}

	if (sceneSelection)
	{
		sceneSelection->ClearSelection();

		for (const std::pair<uint32_t, std::shared_ptr<Tga::SceneObject>>& p : createdObjects)
		{
			sceneSelection->AddToSelection(p.first);
		}
	}

	sceneDocument->SetSceneDirty();

	Tga::SetActiveScene(nullptr);
}

void Forge::CurveTool::PlaceObjectsAlongCurve(Tga::StringId& objectDefinitionName)
{
	Tga::SceneDocument* sceneDocument = Tga::Editor::GetEditor()->GetActiveSceneDocument();
	Tga::Scene* scene = nullptr;
	if (sceneDocument)
	{
		scene = sceneDocument->AccessScene();
	}
	if (scene)
	{
		for (int indexAlongCurve = 0; indexAlongCurve < myNumberOfObjectsToPlace; ++indexAlongCurve)
		{
			const float percentage = static_cast<float>(indexAlongCurve) / static_cast<float>(myNumberOfObjectsToPlace - 1);
			const Tga::Vector3f point = myCurve.GetPointOnCurve(percentage);

			auto object = std::make_shared<Tga::SceneObject>();

			NameObject(scene, object, objectDefinitionName);
			TransformObject(object, point);
			AddObjectToScene(scene, object, sceneDocument);
		}
	}
}

void Forge::CurveTool::PlaceObjectsAlongCurve(std::vector<Tga::StringId>& objectDefinitionNames)
{
	Tga::SceneDocument* sceneDocument = Tga::Editor::GetEditor()->GetActiveSceneDocument();
	Tga::Scene* scene = nullptr;
	if (sceneDocument)
	{
		scene = sceneDocument->AccessScene();
	}
	if (scene)
	{
		const int typeAmount = static_cast<int>(objectDefinitionNames.size());
		int nameIndex = -1;
		for (int indexAlongCurve = 0; indexAlongCurve < myNumberOfObjectsToPlace; ++indexAlongCurve)
		{
			const float percentage = static_cast<float>(indexAlongCurve) / static_cast<float>(myNumberOfObjectsToPlace - 1);
			const Tga::Vector3f point = myCurve.GetPointOnCurve(percentage);
			auto object = std::make_shared<Tga::SceneObject>();

			if (myPlaceDifferentTypesInRandomOrder)
			{
				nameIndex = rand() % (typeAmount);
			}
			else if (!myPlaceDifferentTypesInRandomOrder)
			{
				++nameIndex;
				if (nameIndex >= typeAmount)
				{
					nameIndex = 0;
				}
			}

			NameObject(scene, object, objectDefinitionNames[nameIndex]);
			TransformObject(object, point);
			AddObjectToScene(scene, object, sceneDocument);
		}
	}
}

Tga::Matrix4x4f Forge::CurveTool::GetAlignedTransform(float nextPercentage, const Tga::Vector3f& posOfCurrentObject)
{

	Tga::Matrix4x4f alignedTransform;
	alignedTransform.SetPosition(posOfCurrentObject);

	//this is v and it is the unormalized vector between the current object being placed, and the next one up.
	const Tga::Vector3f forward = (myCurve.GetPointOnCurve(nextPercentage) - posOfCurrentObject).GetNormalized();

	Tga::Vector3f worldUp = { 0.f, 1.f, 0.f };

	if (fabsf(forward.Dot(worldUp)) > 0.999f)
	{
		worldUp = { 1.f, 0.f, 0.f }; // fallback if near parallel
	}

	Tga::Vector3f right = worldUp.Cross(forward).GetNormalized();
	Tga::Vector3f up = forward.Cross(right); // already normalized

	// remember to normalize
	switch (myAlignment)
	{
	case Alignment::Right:
	{
		alignedTransform.SetRight(forward * -1.f);
		alignedTransform.SetUp(up);
		alignedTransform.SetForward(right);
		break;
	}
	case Alignment::Up:
	{
		alignedTransform.SetRight(right);
		alignedTransform.SetUp(forward * -1.f);
		alignedTransform.SetForward(up);
		break;
	}
	case Alignment::Forward:
	{
		alignedTransform.SetRight(right);
		alignedTransform.SetUp(up);
		alignedTransform.SetForward(forward);
		break;
	}
	default:
	{
		break;
	}
	}

	return alignedTransform;




}

Tga::Matrix4x4f Forge::CurveTool::GetAlignedTransformFromLast(const Tga::Vector3f& newForward, const Tga::Vector3f& posOfCurrentObject)
{
	Tga::Matrix4x4f alignedTransform;
	alignedTransform.SetPosition(posOfCurrentObject);

	const Tga::Vector3f forward = newForward.GetNormalized();

	Tga::Vector3f worldUp = { 0.f, 1.f, 0.f };

	if (fabsf(forward.Dot(worldUp)) > 0.999f)
	{
		worldUp = { 1.f, 0.f, 0.f }; // fallback if near parallel
	}

	Tga::Vector3f right = worldUp.Cross(forward).GetNormalized();
	Tga::Vector3f up = forward.Cross(right); // already normalized

	// remember to normalize
	switch (myAlignment)
	{
	case Alignment::Right:
	{
		alignedTransform.SetRight(forward * -1.f);
		alignedTransform.SetUp(up);
		alignedTransform.SetForward(right);
		break;
	}
	case Alignment::Up:
	{
		alignedTransform.SetRight(right);
		alignedTransform.SetUp(forward * -1.f);
		alignedTransform.SetForward(up);
		break;
	}
	case Alignment::Forward:
	{
		alignedTransform.SetRight(right);
		alignedTransform.SetUp(up);
		alignedTransform.SetForward(forward);
		break;
	}
	default:
	{
		break;
	}
	}

	return alignedTransform;

}

void Forge::CurveTool::AlignObjectsAlongCurve(Tga::StringId& objectDefinitionName)
{

	Tga::SceneDocument* sceneDocument = Tga::Editor::GetEditor()->GetActiveSceneDocument();
	Tga::Scene* scene = nullptr;
	if (sceneDocument)
	{
		scene = sceneDocument->AccessScene();
	}
	if (scene)
	{
		for (int indexAlongCurve = 0; indexAlongCurve < myNumberOfObjectsToPlace; ++indexAlongCurve)
		{
			const float currentPercentage = static_cast<float>(indexAlongCurve) / static_cast<float>(myNumberOfObjectsToPlace - 1);
			const Tga::Vector3f point = myCurve.GetPointOnCurve(currentPercentage);

			auto object = std::make_shared<Tga::SceneObject>();
			NameObject(scene, object, objectDefinitionName);
			{
				if (indexAlongCurve < myNumberOfObjectsToPlace - 1)
				{
					const float nextPercentage = static_cast<float>(indexAlongCurve + 1) / static_cast<float>(myNumberOfObjectsToPlace - 1);
					object->SetTransform(GetAlignedTransform(nextPercentage, point));
				}
				else
				{
					const float priorPercentage = static_cast<float>(indexAlongCurve - 1) / static_cast<float>(
						myNumberOfObjectsToPlace - 1);
					Tga::Vector3f directionOfLast = (myCurve.GetPointOnCurve(currentPercentage) - myCurve.GetPointOnCurve(priorPercentage)).
						GetNormalized();
					object->SetTransform(GetAlignedTransformFromLast(directionOfLast, point));
				}
			}
			AddObjectToScene(scene, object, sceneDocument);
		}
	}
}



void Forge::CurveTool::AutoAlignObjects(Tga::StringId& objectDefinitionName)
{
	auto sceneObject = std::make_shared<Tga::SceneObject>();
	sceneObject->SetSceneObjectDefinitionName(objectDefinitionName);

	Tga::SceneObjectDefinitionManager sceneObjectDefinitionManager{};
	sceneObjectDefinitionManager.Init(Tga::Settings::GameAssetRoot().string());

	std::vector<Tga::ScenePropertyDefinition> sceneObjectProperties;

	sceneObject->CalculateCombinedPropertySet(sceneObjectDefinitionManager, sceneObjectProperties);
	SceneLoading::ScenePropertyExtractor props(sceneObjectProperties);

	const Tga::SceneModel* sceneModel = props.GetCopyOnWriteWrapperByType<Tga::SceneModel>();
	const Tga::SceneVertexPaintableModel* paintableSceneModel = props.GetCopyOnWriteWrapperByType<Tga::SceneVertexPaintableModel>();

	Tga::AnimatedModelInstance animatedModelInstance;
	Tga::ModelInstance modelInstance; // { nullptr };
	Tga::BoxSphereBounds bounds;

	if (sceneModel != nullptr)
	{
		if (!sceneModel->isAnimated)
		{
			modelInstance = ReadModelInstance(*sceneModel);
			std::shared_ptr<Tga::Model> model = modelInstance.GetModel();
			bounds = model->GetMeshData(0).Bounds;
		}
		else
		{
			animatedModelInstance = ReadAnimatedModelInstance(*sceneModel);
			std::shared_ptr<Tga::Model> model = animatedModelInstance.GetModel();
			bounds = model->GetMeshData(0).Bounds;
		}
	}
	else if (paintableSceneModel != nullptr)
	{
		//I don't think vertex painted objects will be used on anything other than set dressing (at least, not yet)
		Tga::PaintableModelInstance paintable = ReadPaintableModelInstance(*paintableSceneModel);
		std::shared_ptr<Tga::Model> model = paintable.GetModel();
		bounds = model->GetMeshData(0).Bounds;
	}

	const float curveLength = myCurve.CalculateCurveLengthFastSample();

	float objectSize = 0.f;

	switch (myAlignment)
	{
	case Alignment::Right:
	{
		objectSize = bounds.BoxExtents.x;
		break;
	}
	case Alignment::Up:
	{
		objectSize = bounds.BoxExtents.y;
		break;
	}
	case Alignment::Forward:
	{
		objectSize = bounds.BoxExtents.z;
		break;
	}
	case Alignment::Invalid:
	{
		return;
	}
	case Alignment::count:
	{
		return;
	}
	default:
	{
		return;
	}
	}

	const int objectAmount = static_cast<int>(curveLength / (objectSize + mySpacing));

	Tga::SceneDocument* sceneDocument = Tga::Editor::GetEditor()->GetActiveSceneDocument();
	Tga::Scene* scene = nullptr;
	if (sceneDocument)
	{
		scene = sceneDocument->AccessScene();
	}
	if (scene)
	{
		for (int indexAlongCurve = 0; indexAlongCurve < objectAmount; ++indexAlongCurve)
		{
			const float currentPercentage = static_cast<float>(indexAlongCurve) / static_cast<float>(objectAmount - 1);
			const Tga::Vector3f point = myCurve.GetPointOnCurve(currentPercentage);

			auto object = std::make_shared<Tga::SceneObject>();

			NameObject(scene, object, objectDefinitionName);

			if (indexAlongCurve < myNumberOfObjectsToPlace - 1)
			{
				const float nextPercentage = static_cast<float>(indexAlongCurve + 1) / static_cast<float>(myNumberOfObjectsToPlace - 1);
				object->SetTransform(GetAlignedTransform(nextPercentage, point));
			}
			else
			{
				const float priorPercentage = static_cast<float>(indexAlongCurve - 1) / static_cast<float>(
					myNumberOfObjectsToPlace - 1);
				Tga::Vector3f directionOfLast = (myCurve.GetPointOnCurve(currentPercentage) - myCurve.GetPointOnCurve(priorPercentage)).
					GetNormalized();
				object->SetTransform(GetAlignedTransformFromLast(directionOfLast, point));
			}

			AddObjectToScene(scene, object, sceneDocument);
		}
	}

}

Tga::ModelInstance Forge::CurveTool::ReadModelInstance(const Tga::SceneModel& aSceneModel)
{
	Tga::ModelInstance instance;
	Tga::StringId path = aSceneModel.path;

	if (aSceneModel.renderMode == Tga::RenderMode::None || path.IsEmpty() || Tga::Settings::ResolveAssetPath(path.GetString()).empty())
	{
		return instance;
	}

	auto& engine = *Tga::Engine::GetInstance();
	auto& textureManager = engine.GetTextureManager();

	if (Tga::ModelFactory::GetInstance().GetModel(path.GetString()))
	{
		instance = Tga::ModelFactory::GetInstance().GetModelInstance(path.GetString());
		int meshCount = std::min(static_cast<int>(instance.GetModel()->GetMeshCount()), MAX_MESHES_PER_MODEL);

		for (int i = 0; i < meshCount; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				if (!aSceneModel.textures[i][j].IsEmpty())
				{
					// diffuse texture should be srgb, the rest not
					Tga::TextureSrgbMode srgbMode = (j == 0)
						? Tga::TextureSrgbMode::ForceSrgbFormat
						: Tga::TextureSrgbMode::ForceNoSrgbFormat;

					if (Tga::Texture* texture = textureManager.GetTexture(aSceneModel.textures[i][j].GetString(), srgbMode))
					{
						instance.SetTexture(i, j, texture);

						if (j == 0) // albedo
						{
							instance.GetModel()->SetIsDefaultAlbedoTexture(false);
						}
					}
				}
			}
		}

		instance.SetIsCullable(aSceneModel.cullable);
	}

	return instance;
}

Tga::AnimatedModelInstance Forge::CurveTool::ReadAnimatedModelInstance(const Tga::SceneModel& aSceneModel)
{
	Tga::AnimatedModelInstance instance;
	Tga::StringId path = aSceneModel.path;

	if (aSceneModel.renderMode == Tga::RenderMode::None || path.IsEmpty() || Tga::Settings::ResolveAssetPath(path.GetString()).empty())
	{
		return {};
	}

	auto& engine = *Tga::Engine::GetInstance();
	auto& textureManager = engine.GetTextureManager();

	if (Tga::ModelFactory::GetInstance().GetModel(path.GetString()))
	{
		instance = Tga::ModelFactory::GetInstance().GetAnimatedModelInstance(path.GetString());
		int meshCount = std::min(static_cast<int>(instance.GetModel()->GetMeshCount()), MAX_MESHES_PER_MODEL);

		for (int i = 0; i < meshCount; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				if (!aSceneModel.textures[i][j].IsEmpty())
				{
					// diffuse texture should be srgb, the rest not
					Tga::TextureSrgbMode srgbMode = (j == 0)
						? Tga::TextureSrgbMode::ForceSrgbFormat
						: Tga::TextureSrgbMode::ForceNoSrgbFormat;

					if (Tga::Texture* texture = textureManager.GetTexture(aSceneModel.textures[i][j].GetString(), srgbMode))
					{
						instance.SetTexture(i, j, texture);

						if (j == 0) // albedo
						{
							instance.GetModel()->SetIsDefaultAlbedoTexture(false);
						}
					}
				}
			}
		}
	}
	return instance;
}

Tga::PaintableModelInstance Forge::CurveTool::ReadPaintableModelInstance(const Tga::SceneVertexPaintableModel& aPaintableModelValue)
{
	Tga::StringId path = aPaintableModelValue.path;

	if (path.IsEmpty() || Tga::Settings::ResolveAssetPath(path.GetString()).empty())
	{
		return {};
	}

	Tga::PaintableModelInstance instance;

	if (std::shared_ptr<Tga::Model> model = Tga::ModelFactory::GetInstance().GetModel(path.GetString()))
	{
		instance.Init(model);

		int meshCount = std::min(static_cast<int>(instance.GetModel()->GetMeshCount()), MAX_MESHES_PER_MODEL);

		auto& engine = *Tga::Engine::GetInstance();
		auto& textureManager = engine.GetTextureManager();

		for (int i = 0; i < meshCount; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				if (!aPaintableModelValue.baseTextures[i][j].IsEmpty())
				{
					// diffuse texture should be srgb, the rest not
					Tga::TextureSrgbMode srgbMode = (j == 0)
						? Tga::TextureSrgbMode::ForceSrgbFormat
						: Tga::TextureSrgbMode::ForceNoSrgbFormat;

					if (Tga::Texture* texture = textureManager.GetTexture(aPaintableModelValue.baseTextures[i][j].GetString(), srgbMode))
					{
						instance.SetTexture(i, j, texture);
					}
				}
			}
		}

		for (int i = 0; i < static_cast<int>(Tga::VertexPaintChannel::Count); i++)
		{
			for (int j = 0; j < static_cast<int>(Tga::VertexPaintTexture::Count); j++)
			{
				if (!aPaintableModelValue.paintTextures[i][j].IsEmpty())
				{
					Tga::TextureSrgbMode srgbMode = (j == 0)
						? Tga::TextureSrgbMode::ForceSrgbFormat
						: Tga::TextureSrgbMode::ForceNoSrgbFormat;

					if (Tga::Texture* texture = textureManager.GetTexture(aPaintableModelValue.paintTextures[i][j].GetString(), srgbMode))
					{
						instance.SetVertexPaintTextures(static_cast<Tga::VertexPaintChannel>(i), static_cast<Tga::VertexPaintTexture>(j),
							texture);
					}
				}
			}
		}

		if (!aPaintableModelValue.paintWeightValues.empty())
		{
			instance.SetVertexPaintWeights(aPaintableModelValue.paintWeightValues);
		}
	}

	return instance;
}

void Forge::CurveTool::HelpMarker(const char* aDescription, bool aSameLine)
{
	if (aSameLine)
	{
		ImGui::SameLine();
	}

	ImGui::TextDisabled(ICON_LC_CIRCLE_HELP);
	if (ImGui::BeginItemTooltip())
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(aDescription);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void Forge::CurveTool::MoveCurveInFrontOfCamera()
{
	Tga::SceneDocument* scene = Tga::Editor::GetEditor()->GetActiveSceneDocument();
	if (scene != nullptr)
	{
		/*const Tga::Vector3f cameraForward = scene->GetViewport().GetCamera().GetTransform().GetForward();
		const Tga::Vector3f cameraPos = scene->GetViewport().GetCamera().GetTransform().GetPosition();*/
		std::vector<Tga::Vector3f> internalPointVectorRelations;

		// determine relation in between points so that they can be transferred to new origin.
		for (int i = 1; i < myCurve.AccessCurvePoints().size(); ++i)
		{
			internalPointVectorRelations.emplace_back(myCurve.AccessCurvePoints()[i] - myCurve.AccessCurvePoints()[0]);
		}

		myCurve.AccessCurvePoints()[0] = GetPositionInFrontOfCamera(); /*cameraPos + cameraForward * myAheadOfCamera;*/

		for (int i = 1; i < myCurve.AccessCurvePoints().size(); ++i)
		{
			myCurve.AccessCurvePoints()[i] = myCurve.AccessCurvePoints()[0] + internalPointVectorRelations[i - 1];
		}
	}
}

void Forge::CurveTool::EditPoints()
{
	Tga::SceneDocument* scene = Tga::Editor::GetEditor()->GetActiveSceneDocument();
	if (scene == nullptr)
	{
		return;
	}

	// By gizmo
	static std::string pointName;
	pointName = "";

	ImGui::Text("Point Positions");
	ImGui::Separator();

	ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
	ImGui::Text(ICON_LC_MOVE_3D);
	ImGui::PopFont();
	ImGui::SetItemTooltip("Move base points with gizmo");

	for (uint8_t indexToMove = 0; indexToMove < myCurve.AccessCurvePoints().size(); ++indexToMove)
	{
		pointName = "Gizmo to Point " + std::to_string(indexToMove + 1);
		const Tga::Vector4f blendedColor = FMath::Lerp(myCurve.GetColorA(), myCurve.GetColorB(),
			static_cast<float>(indexToMove) / static_cast<float>(myCurve.AccessCurvePoints().
				size()));

		ImVec4 baseColor;
		baseColor.x = blendedColor.x;
		baseColor.y = blendedColor.y;
		baseColor.z = blendedColor.z;
		baseColor.w = blendedColor.w;

		const ImVec4 hoveredColor = baseColor + ImVec4(.1f, .1f, .1f, 0.f);
		ImVec4 activeColor = baseColor + ImVec4(.2f, .2f, .2f, 0.f);
		if (indexToMove == myPointToEditIndex)
		{
			baseColor = baseColor + ImVec4(.3f, .3f, .3f, .3f);
		}

		ImGui::PushStyleColor(ImGuiCol_Button, baseColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

		if (ImGui::Button(pointName.c_str()))
		{
			myPointToEditIndex = indexToMove;
		}
		ImGui::PopStyleColor(3);
		if (indexToMove < myCurve.AccessCurvePoints().size() - 1)
		{
			ImGui::SameLine();
		}
	}

	if (ImGui::IsKeyPressed(ImGuiKey_1))
	{
		myPointToEditIndex = 0;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_2))
	{
		myPointToEditIndex = 1;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_3))
	{
		myPointToEditIndex = 2;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_4))
	{
		myPointToEditIndex = 3;
	}

	// World/local -space mode
	ImGui::Text(" Gizmo Translation Snap: ");
	ImGui::SameLine();
	ImGui::Checkbox("Snap  Enabled", &mySnap.snapPos);
	ImGui::SameLine();
	ImGui::PushItemWidth(100.f);
	ImGui::DragFloat("Snap Amount", &mySnap.pos);
	ImGui::PopItemWidth();

	// by drag float 3
	ImGui::Separator();
	int pointNumber = 1;
	static std::vector<ImVec4> debugColors;
	debugColors.clear();
	for (auto& point : myCurve.AccessCurvePoints())
	{
		pointName = "Point " + std::to_string(pointNumber);
		const float percentage = static_cast<float>(pointNumber) / static_cast<float>(myCurve.AccessCurvePoints().size());
		const Tga::Vector4f color = FMath::Lerp(myCurve.GetColorA(), myCurve.GetColorB(), percentage);

		debugColors.emplace_back(color.x, color.y, color.z, color.w);
		ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(color.x, color.y, color.z, color.w);
		ImGui::DragFloat3(pointName.c_str(), &point.x, 1.f, -5000000.f, FLT_MAX, "%.1f");
		++pointNumber;
	}
	ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(1.f, 1.f, 1.f, 1.f);

	ImGui::Separator();
	// by snapping or locking  to camera 
	if (ImGui::BeginTable("Points in relation to camera", 2))
	{
		ImGui::TableSetupColumn("Snap to camera");
		ImGui::TableSetupColumn("Lock to camera");

		static bool boolLocks[4];

		for (int row = 0; row < myCurve.AccessCurvePoints().size(); row++)
		{
			ImGui::TableNextRow();
			for (int column = 0; column < 2; column++)
			{
				ImGui::TableSetColumnIndex(column);

				if (column == 1)
				{
					ImGui::GetStyle().Colors[ImGuiCol_Text] = debugColors[row];
					pointName = "Snap P" + std::to_string(row + 1) + " to camera";

					if (ImGui::Button(pointName.c_str()))
					{
						myCurve.AccessCurvePoints()[row] = GetPositionInFrontOfCamera(); /*getPosInFrontOfCam()*/
					}
				}
				else
				{
					ImGui::GetStyle().Colors[ImGuiCol_Text] = debugColors[row];
					pointName = "Lock P" + std::to_string(row + 1) + " to camera";
					ImGui::Checkbox(pointName.c_str(), &boolLocks[row]);

					if (boolLocks[row])
					{
						myCurve.AccessCurvePoints()[row] = GetPositionInFrontOfCamera(); /* getPosInFrontOfCam()*/
					}
				}
			}
		}
		ImGui::EndTable();
	}

	ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(1.f, 1.f, 1.f, 1.f);

	ImGui::Separator();

	// Snap whole structure in front of camera
	{
		ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
		if (ImGui::Button(ICON_LC_SWITCH_CAMERA))
		{
			MoveCurveInFrontOfCamera();
		}
		ImGui::PopFont();
		ImGui::SetItemTooltip("Move whole intact curve in front of camera");
		ImGui::SameLine();
		ImGui::Checkbox("Render replacement-aim", &myRenderCameraAim);
		ImGui::SameLine();
		ImGui::SameLine();
		ImGui::PushItemWidth(100.f);
		ImGui::DragFloat("this much ahead", &myAheadOfCamera, 5.f, 0.f, 100000.f, "%.1f");
		ImGui::PopItemWidth();
	}
	ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
	if (ImGui::Button(ICON_LC_PLANE_LANDING))
	{
		for (auto& point : myCurve.AccessCurvePoints())
		{
			point.y = 0.f;
		}
	}
	ImGui::PopFont();
	ImGui::SetItemTooltip("Flatten vertically, and set all Y-coordinates to zero");

	ImGui::SameLine();
	ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
	if (ImGui::Button(ICON_LC_PLANE_TAKEOFF))
	{
		const float newY = GetPositionInFrontOfCamera().y;

		for (auto& point : myCurve.AccessCurvePoints())
		{
			point.y = newY;
		}
	}
	ImGui::PopFont();
	ImGui::SetItemTooltip("Flatten vertically, and set all Y-coordinates to camera height");

	ImGui::SameLine();
	ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
	if (ImGui::Button(ICON_LC_SLASH))
	{
		const Tga::Vector3f firstPoint = myCurve[0];
		const Tga::Vector3f endPoint = myCurve[3];

		for (int i = 1; i < myCurve.AccessCurvePoints().size() - 1; ++i)
		{
			const float percentage = static_cast<float>(i) / static_cast<float>(myCurve.AccessCurvePoints().size() - 1);
			myCurve[static_cast<uint8_t>(i)] = FMath::Lerp(firstPoint, endPoint, percentage);
		}
	}
	ImGui::PopFont();
	ImGui::SetItemTooltip("Make a straight line between first and last point");

	if (ImGui::Button("Reset Curve To World Origin"))
	{
		myCurve[0] = { 0.f, 0.f, 0.f };
		myCurve[1] = { 0.f, 500.f, 0.f };
		myCurve[2] = { 500.f, 500.f, 0.f };
		myCurve[3] = { 500.f, 0.f, 0.f };
	}
	ImGui::SetItemTooltip("Resets curve point close to world origin");
}

void Forge::CurveTool::Debug()
{
	static int lineFidelity = 16;

	if (ImGui::SliderInt("Debug Curve Fidelity", &lineFidelity, 4, 128))
	{
		myCurve.SetDebugDrawFidelity(lineFidelity);
	}

	ImGui::Text("Debug Colors");
	static Tga::Vector4f colorA = myCurve.GetColorA();
	ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(colorA.x, colorA.y, colorA.z, colorA.w);
	if (ImGui::ColorPicker4("Color A", &colorA.x))
	{
		myCurve.SetColorA(colorA);
	}

	static Tga::Vector4f colorB = myCurve.GetColorB();
	ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(colorB.x, colorB.y, colorB.z, colorB.w);
	if (ImGui::ColorPicker4("Color B", &colorB.x))
	{
		myCurve.SetColorB(colorB);
	}

	ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(1.f, 1.f, 1.f, 1.f);
}

Tga::Vector3f Forge::CurveTool::GetPositionInFrontOfCamera()
{
	Tga::SceneDocument* scene = Tga::Editor::GetEditor()->GetActiveSceneDocument();

	if (scene != nullptr)
	{
		const Tga::Vector3f cameraForward = scene->GetViewport().GetCamera().GetTransform().GetForward();
		const Tga::Vector3f cameraPos = scene->GetViewport().GetCamera().GetTransform().GetPosition();
		return (cameraPos + cameraForward * myAheadOfCamera);
	}

	return Tga::Vector3f::Zero;
}

void Forge::CurveTool::UpdateTranslation(uint8_t aCurvePointIndex, const Tga::Vector3f& referencePosition, const Tga::Matrix4x4f& transform)
{
	Tga::Vector3f& curvePoint = myCurve[aCurvePointIndex];

	Tga::Matrix4x4f oldTransform = Tga::Matrix4x4f::CreateFromTranslation(curvePoint);

	oldTransform.SetPosition(oldTransform.GetPosition() - referencePosition);

	Tga::Matrix4x4f newTransform = oldTransform * transform;

	newTransform.SetPosition(newTransform.GetPosition() + referencePosition);

	curvePoint = newTransform.GetPosition();
}

void Forge::CurveTool::DrawGizmos(const Tga::Camera& camera, [[maybe_unused]] Tga::ViewportInterface& aViewportInterface,
	[[maybe_unused]] Tga::Vector2i aViewportPos, [[maybe_unused]] Tga::Vector2i aViewportSize)
{
	ImGuizmo::SetID(ImGui::GetID(0));
	ImGui::SetItemDefaultFocus();
	auto io = ImGui::GetIO();

	if (!ImGuizmo::IsUsing())
	{
		Tga::Vector3f curvePos = myCurve[myPointToEditIndex];

		if (mySnap.snapPos)
		{
			curvePos = curvePos / mySnap.pos;
			curvePos.x = round(curvePos.x);
			curvePos.y = round(curvePos.y);
			curvePos.z = round(curvePos.z);

			curvePos = mySnap.pos * curvePos;
		}

		myManipulationStartPos = curvePos;
	}

	Tga::Matrix4x4f cameraToWorld = camera.GetTransform();
	cameraToWorld.SetPosition(cameraToWorld.GetPosition() - myManipulationStartPos);
	Tga::Matrix4x4f view = Tga::Matrix4x4f::GetFastInverse(cameraToWorld);
	Tga::Matrix4x4f proj = camera.GetProjection();

	float left = static_cast<float>(aViewportPos.x);
	float top = static_cast<float>(aViewportPos.y);
	float width = static_cast<float>(aViewportSize.x);
	float height = static_cast<float>(aViewportSize.y);

	ImGuizmo::SetRect(left, top, width, height);
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

	Tga::Matrix4x4f transformBefore = myManipulationCurrentTransform;
	Tga::Matrix4x4f transformAfter;

	float snap[3];
	snap[0] = snap[1] = snap[2] = (!mySnap.snapPos != !io.KeyCtrl) ? mySnap.pos : 0.f;

	transformBefore.SetPosition(myCurve[myPointToEditIndex] - myManipulationStartPos);

	transformAfter = transformBefore;

	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

	ImGuizmo::Manipulate(view.GetDataPtr(), proj.GetDataPtr(), ImGuizmo::TRANSLATE, ImGuizmo::WORLD, transformAfter.GetDataPtr(), nullptr,
		snap);

	myManipulationCurrentTransform = transformAfter;

	if (!io.KeyAlt)
	{
		if (ImGuizmo::IsOver() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			myManipulationInitialTransform = transformBefore;
			myIsManipulating = true;
		}

		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && myIsManipulating)
		{
			if (!ImGui::IsItemHovered())
			{
				int a = 2;
				a = 3;
			}
			Tga::Vector3f moveVec = transformAfter.GetPosition() - transformBefore.GetPosition();
			myCurve[myPointToEditIndex] += moveVec;
		}

		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			Tga::Vector3f pos, scale;
			Tga::Quaternionf rot;
			transformAfter.DecomposeMatrix(pos, rot, scale);
			Tga::Vector3f euler = rot.GetYawPitchRoll();
			euler;
			myIsManipulating = false;
		}
	}
}

void Forge::CurveTool::ClearUndo()
{
	myIdOfRecentlyAddedObjects.clear();
}
