#include "ResourcesLoader.hpp"
#include "viewportLogic/UIPanels/FrameDataCatcher.hpp"
#include "../ThirdParty/json.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#define NOMINMAX
#include <Windows.h>

using json = nlohmann::json;

// ==================== SimpleResourceHandler ====================

SimpleResourceHandler::SimpleResourceHandler(const std::string& mimeType, const std::string& content)
	: mime_type_(mimeType)
	, content_(content)
	, offset_(0)
{
}

bool SimpleResourceHandler::Open(CefRefPtr<CefRequest> request, bool& handle_request, CefRefPtr<CefCallback> callback)
{
	handle_request = true;
	return true;
}

void SimpleResourceHandler::GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& response_length, CefString& redirectUrl)
{
	response->SetMimeType(mime_type_);
	response->SetStatus(200);
	response_length = content_.size();
}

bool SimpleResourceHandler::Read(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback> callback)
{
	bytes_read = 0;
	
	if (offset_ < content_.size())
	{
		int transfer_size = (std::min)(bytes_to_read, static_cast<int>(content_.size() - offset_));
		memcpy(data_out, content_.data() + offset_, transfer_size);
		offset_ += transfer_size;
		bytes_read = transfer_size;
		return true;
	}
	
	return false;
}

void SimpleResourceHandler::Cancel()
{
	// Nothing to cancel
}

// ==================== LocalResourceRequestHandler ====================

LocalResourceRequestHandler::LocalResourceRequestHandler()
{
	char buffer[MAX_PATH];
	GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	std::string exePath(buffer);
	size_t lastSlash = exePath.find_last_of("\\/");
	if (lastSlash != std::string::npos)
	{
		base_dir_ = exePath.substr(0, lastSlash + 1);
	}
	else
	{
		base_dir_ = "";
	}
	std::cout << "[LocalResourceRequestHandler] Constructor: base_dir_=\"" << base_dir_ << "\"" << std::endl;
	preloadResources();
}

CefRefPtr<CefResourceHandler> LocalResourceRequestHandler::GetResourceHandler(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request)
{
	std::string url = request->GetURL().ToString();

	if (url == "http://localhost:8080/api/uipanels/update" || url == "http://localhost:8080/api/iframes/update")
	{
		return handleUIPanelUpdate(request);
	}

	if (url.find("http://localhost:8080/api/") == 0)
	{
		return nullptr;
	}

	if (url.find("http://localhost:8080/ui/") == 0)
	{
		std::string filePath = url.substr(std::string("http://localhost:8080/ui/").length());
		return serveLocalFile(filePath);
	}
	
	return nullptr;
}

CefRefPtr<CefResourceHandler> LocalResourceRequestHandler::handleUIPanelUpdate(CefRefPtr<CefRequest> request)
{
	std::string url = request->GetURL().ToString();
	
	CefRefPtr<CefPostData> postData = request->GetPostData();
	if (!postData)
	{
		std::cout << "[LocalResourceRequestHandler] ERROR: No POST data" << std::endl;
		return new SimpleResourceHandler("application/json", "{\"success\": false, \"error\": \"No POST data\"}");
	}
	
	CefPostData::ElementVector elements;
	postData->GetElements(elements);
	
	std::string body;
	for (size_t i = 0; i < elements.size(); ++i)
	{
		CefRefPtr<CefPostDataElement> element = elements[i];
		if (element->GetType() == PDE_TYPE_BYTES)
		{
			size_t size = element->GetBytesCount();
			if (size > 0)
			{
				std::vector<char> buffer(size);
				element->GetBytes(size, buffer.data());
				body.append(buffer.data(), size);
			}
		}
	}
	
	if (!json::accept(body))
	{
		std::cout << "[LocalResourceRequestHandler] ERROR: Invalid JSON" << std::endl;
		return new SimpleResourceHandler("application/json", "{\"success\": false, \"error\": \"Invalid JSON\"}");
	}
	
	json requestData = json::parse(body, nullptr, false);
	if (requestData.is_discarded())
	{
		std::cout << "[LocalResourceRequestHandler] ERROR: JSON parse failed" << std::endl;
		return new SimpleResourceHandler("application/json", "{\"success\": false, \"error\": \"JSON parse failed\"}");
	}
	
	if (!requestData.contains("iframes") || !requestData["iframes"].is_array())
	{
		return new SimpleResourceHandler("application/json", "{\"success\": false, \"error\": \"Missing iframes array\"}");
	}
	
	FrameDataCatcher* catcher = FrameDataCatcher::getInstance();
	if (!catcher)
	{
		return new SimpleResourceHandler("application/json", "{\"success\": false, \"error\": \"FrameDataCatcher not available\"}");
	}
	
	std::map<std::string, IFrameData> uiPanelIFrames;
	
	for (const auto& iframe : requestData["iframes"])
	{
		if (iframe.contains("name") && iframe.contains("x") && iframe.contains("y") &&
			iframe.contains("width") && iframe.contains("height"))
		{
			std::string name = iframe["name"];
			if (name == "viewport_panel")
			{
				continue;
			}
			
			IFrameData data;
			data.name = name;
			data.x = iframe["x"];
			data.y = iframe["y"];
			data.width = iframe["width"];
			data.height = iframe["height"];
			data.clientX = iframe.contains("clientX") ? iframe["clientX"].get<int>() : data.x;
			data.clientY = iframe.contains("clientY") ? iframe["clientY"].get<int>() : data.y;
			uiPanelIFrames[name] = data;
			
			catcher->iframe_data_map_[name] = data;
		}
	}
			
	std::string response = "{\"success\": true, \"count\": " + std::to_string(uiPanelIFrames.size()) + "}";
	return new SimpleResourceHandler("application/json", response);
}

std::string LocalResourceRequestHandler::loadFileContent(const std::string& filePath)
{
	std::string fullPath = base_dir_ + "ui/" + filePath;
	std::ifstream file(fullPath, std::ios::binary);
	
	if (!file.is_open())
	{
		fullPath = "ui/" + filePath;
		file.open(fullPath, std::ios::binary);
	}
	
	if (!file.is_open())
	{
		return "";
	}
	
	std::stringstream buffer;
	buffer << file.rdbuf();
	file.close();
	
	return buffer.str();
}

void LocalResourceRequestHandler::preloadResources()
{
	std::vector<std::string> resources = {
		"main_layout.html",
		"main_layout.css",
		"layout_resizer.js",
		"main_layout_manager.js",
		"hierarchy_inspector.html",
		"hierarchy_inspector.css",
		"hierarchy_inspector.js",
		"Viewport_docking.html",
		"object_inspector.html",
		"history_logger.html",
		"project_viewer.html",
		"project_viewer.css",
		"ribbon_menu.css",
		"console_server.html",
		"console_server.css",
		"console_server.js"
	};
	
	std::cout << "[LocalResourceRequestHandler] Preload START: " << resources.size() << " resources" << std::endl;
	
	for (const auto& resource : resources)
	{
		std::string content = loadFileContent(resource);
		if (!content.empty())
		{
			resource_cache_[resource] = content;
			std::cout << "[LocalResourceRequestHandler] CACHED: " << resource << " (" << content.size() << " bytes)" << std::endl;
		}
	}
	
	std::cout << "[LocalResourceRequestHandler] Preload COMPLETE: " << resource_cache_.size() << " files" << std::endl;
}

CefRefPtr<CefResourceHandler> LocalResourceRequestHandler::serveLocalFile(const std::string& filePath)
{
	auto it = resource_cache_.find(filePath);
	if (it != resource_cache_.end())
	{
		std::string mimeType = getMimeType(filePath);
		return new SimpleResourceHandler(mimeType, it->second);
	}
	
	std::string content = loadFileContent(filePath);
	if (content.empty())
	{
		std::cout << "[LocalResourceRequestHandler] ERROR: File not found: " << filePath << std::endl;
		return nullptr;
	}
	
	resource_cache_[filePath] = content;
	std::string mimeType = getMimeType(filePath);
	return new SimpleResourceHandler(mimeType, content);
}

std::string LocalResourceRequestHandler::getMimeType(const std::string& filePath) const
{
	if (filePath.find(".css") != std::string::npos)
	{
		return "text/css";
	}
	else if (filePath.find(".js") != std::string::npos)
	{
		return "application/javascript";
	}
	else if (filePath.find(".png") != std::string::npos)
	{
		return "image/png";
	}
	else if (filePath.find(".jpg") != std::string::npos || filePath.find(".jpeg") != std::string::npos)
	{
		return "image/jpeg";
	}
	else if (filePath.find(".svg") != std::string::npos)
	{
		return "image/svg+xml";
	}
	else if (filePath.find(".json") != std::string::npos)
	{
		return "application/json";
	}
	
	return "text/html";  // Default MIME type
}

// ==================== ResourcesLoader ====================

CefRefPtr<CefResourceRequestHandler> ResourcesLoader::createRequestHandler()
{
	return new LocalResourceRequestHandler();
}
