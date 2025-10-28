#pragma once

// Standard Library
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <set>
#include <array>
#include <chrono>
#include <functional>
#include <cmath>

// Windows specific - MUST be before GLAD to avoid APIENTRY conflict
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// Third Party - GLAD (must be before GLFW)
#include <glad/glad.h>

// Third Party - GLFW
#include <GLFW/glfw3.h>

// Third Party - ImGui
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

// Third Party - ImGuizmo
#include "ImGuizmo.h"

// Third Party - GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Third Party - JSON
#include "json.hpp"
