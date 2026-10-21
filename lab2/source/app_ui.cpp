#include "app_ui.hpp"
#include "app_shared.hpp"

#include <imgui.h>
#include <glm/gtc/constants.hpp>
#include <string>

namespace application {

void drawUI() {
	ImGui::Begin("Scene");

	drawMarkerSettings();
	drawSpotLightsPanel();
	drawPointLightsPanel();
	drawDirectionalLightPanel();
	drawCameraPanel();
	drawAnimationPanel();
	drawObjectsPanel();

	ImGui::End();
}

void drawMarkerSettings() {
	ImGui::Checkbox("Show Point Markers", &ui_show_point_lights);
	ImGui::Checkbox("Show Spot Markers", &ui_show_spot_lights);
}

void drawSpotLightsPanel() {
	if (!ImGui::CollapsingHeader("Spot Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	if (ImGui::Button("Add Spot Light")) {
		if (spot_lights.size() < MAX_SPOT_LIGHTS) {
			SpotLight new_light = {
				{ 0.0f, 4.0f, 2.0f },
				20.0f,
				{ 0.0f, -1.0f, -1.0f },
				glm::cos(glm::radians(20.0f)),
				{ 1.0f, 1.0f, 1.0f },
				glm::cos(glm::radians(30.0f))
			};
			spot_lights.push_back(new_light);
		}
	}
	ImGui::Separator();

	for (size_t i = 0; i < spot_lights.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		std::string header = "Spot Light " + std::to_string(i);
		
		if (ImGui::TreeNode(header.c_str())) {
			ImGui::DragFloat3("Position", &spot_lights[i].position.x, 0.1f, -10.0f, 10.0f, "%.2f");
			ImGui::DragFloat3("Direction", &spot_lights[i].direction.x, 0.05f, -1.0f, 1.0f, "%.2f");
			ImGui::ColorEdit3("Color", &spot_lights[i].color.x);
			ImGui::DragFloat("Intensity", &spot_lights[i].intensity, 0.5f, 0.0f, 100.0f, "%.1f");

			float inner_deg = glm::degrees(glm::acos(spot_lights[i].inner_angle_cos));
			float outer_deg = glm::degrees(glm::acos(spot_lights[i].outer_angle_cos));

			if (ImGui::SliderFloat("Inner Angle", &inner_deg, 1.0f, 89.0f, "%.1f deg")) {
				spot_lights[i].inner_angle_cos = glm::cos(glm::radians(inner_deg));
			}
			if (ImGui::SliderFloat("Outer Angle", &outer_deg, 1.0f, 89.0f, "%.1f deg")) {
				spot_lights[i].outer_angle_cos = glm::cos(glm::radians(outer_deg));
			}

			if (ImGui::Button("Remove")) {
				spot_lights.erase(spot_lights.begin() + i);
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

void drawPointLightsPanel() {
	if (!ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	if (ImGui::Button("Add Point Light")) {
		if (point_lights.size() < MAX_POINT_LIGHTS) {
			PointLight new_light = { { 0.0f, 3.0f, 0.0f }, 15.0f, { 1.0f, 1.0f, 1.0f }, 0.0f };
			point_lights.push_back(new_light);
		}
	}
	ImGui::Separator();

	for (size_t i = 0; i < point_lights.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		std::string header = "Point Light " + std::to_string(i);
		
		if (ImGui::TreeNode(header.c_str())) {
			ImGui::DragFloat3("Position", &point_lights[i].position.x, 0.1f, -10.0f, 10.0f, "%.2f");
			ImGui::ColorEdit3("Color", &point_lights[i].color.x);
			ImGui::DragFloat("Intensity", &point_lights[i].intensity, 0.5f, 0.0f, 100.0f, "%.1f");

			if (ImGui::Button("Remove")) {
				point_lights.erase(point_lights.begin() + i);
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

void drawDirectionalLightPanel() {
	if (!ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	ImGui::DragFloat3("Direction", &ui_directional_light_direction.x, 0.01f, -10.0f, 10.0f, "%.2f");
	ImGui::ColorEdit3("Light Color", &ui_directional_light_color.x);
	ImGui::ColorEdit3("Ambient", &ui_ambient_light.x);
}

void drawCameraPanel() {
	if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	ImGui::DragFloat3("Position", &ui_camera_position.x, 0.1f, -20.0f, 20.0f, "%.2f");
	ImGui::SliderFloat("Yaw", &ui_camera_yaw, -180.0f, 180.0f, "%.1f deg");
	ImGui::SliderFloat("Pitch", &ui_camera_pitch, -89.0f, 89.0f, "%.1f deg");
	ImGui::SliderFloat("FOV", &ui_fov_deg, 20.0f, 120.0f, "%.0f deg");

	if (ImGui::Button("Reset Camera")) {
		ui_camera_position = glm::vec3(0.0f, 0.0f, 3.0f);
		ui_camera_yaw = -90.0f;
		ui_camera_pitch = 0.0f;
		ui_fov_deg = 60.0f;
	}
}

void drawAnimationPanel() {
	if (!ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	ImGui::Checkbox("Playing", &ui_animation.playing);
	ImGui::SliderFloat("Speed", &ui_animation.speed, 0.0f, 5.0f, "%.2f");
	ImGui::SliderFloat("Radius", &ui_animation.radius, 0.2f, 3.0f, "%.2f");
	ImGui::SliderFloat("Height", &ui_animation.height, 0.0f, 2.0f, "%.2f");
	ImGui::SliderFloat("Spin", &ui_animation.spin_speed, 0.0f, 360.0f, "%.0f deg/s");

	if (ImGui::Button("Reset")) {
		ui_animation_time = 0.0f;
	}

	if (ui_animation.playing) {
		const float dt = ImGui::GetIO().DeltaTime;
		ui_animation_time += dt * ui_animation.speed;

		const float t = ui_animation_time;
		const float r = ui_animation.radius;
		const float h = ui_animation.height;

		ui_transform[0].position = glm::vec3(
			r * std::cos(t),
			h * std::sin(2.0f * t),
			r * std::sin(t)
		);

		const float spin = ui_animation.spin_speed * t;
		ui_transform[0].rotation_deg = glm::vec3(
			spin * 0.7f,
			spin,
			spin * 0.3f
		);
	}
}

void drawObjectsPanel() {
	for (uint32_t i = 0; i < object_count; ++i) {
		Transform& t = ui_transform[i];
		std::string header = "Object " + std::to_string(i);
		
		if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PushID(int(i));

			ImGui::Text("Material");
			ImGui::ColorEdit3("Albedo", &ui_color[i].x);
			ImGui::ColorEdit3("Specular", &ui_specular[i].x);
			ImGui::SliderFloat("Shininess", &ui_alpha[i], 1.0f, 256.0f, "%.0f");

			ImGui::Separator();
			ImGui::Text("Transform");
			ImGui::DragFloat3("Position", &t.position.x, 0.01f, -10.0f, 10.0f, "%.2f");
			ImGui::DragFloat3("Rotation", &t.rotation_deg.x, 1.0f, -360.0f, 360.0f, "%.0f deg");
			ImGui::DragFloat3("Scale",    &t.scale.x,    0.01f,   0.05f,  5.0f, "%.2f");

			ImGui::PopID();
		}
	}
}

} // namespace application