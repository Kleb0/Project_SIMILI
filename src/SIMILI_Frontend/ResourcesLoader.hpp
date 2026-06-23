#pragma once

#include "include/cef_resource_handler.h"
#include "include/cef_resource_request_handler.h"
#include "include/cef_request.h"
#include "include/cef_response.h"
#include "include/wrapper/cef_helpers.h"
#include <string>

// Forward declarations
class FrameDataCatcher;

/**
 * @brief Simple handler to serve static content to CEF
 * 
 * Serves file content or JSON responses with proper MIME types
 */
class SimpleResourceHandler : public CefResourceHandler
{
public:
	SimpleResourceHandler(const std::string& mimeType, const std::string& content);

	bool Open(CefRefPtr<CefRequest> request, bool& handle_request, CefRefPtr<CefCallback> callback) override;
	void GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& response_length, CefString& redirectUrl) override;
	bool Read(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback> callback) override;
	void Cancel() override;

private:
	std::string mime_type_;
	std::string content_;
	size_t offset_;
	
	IMPLEMENT_REFCOUNTING(SimpleResourceHandler);
};

/**
 * @brief Handles CEF resource requests by intercepting URLs and serving local files
 * 
 * Provides three behaviors:
 * 1. Intercept /api/uipanels/update → Parse POST data and call C++ functions directly (CEF OSR can't make HTTP requests)
 * 2. Intercept /ui/* → Serve files from disk (resolved from executable directory)
 * 3. Let other API requests through (though they won't work in CEF OSR mode)
 */
class LocalResourceRequestHandler : public CefResourceRequestHandler
{
public:
	LocalResourceRequestHandler();

	CefRefPtr<CefResourceHandler> GetResourceHandler(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefRequest> request) override;

	void preloadResources();

private:
	CefRefPtr<CefResourceHandler> handleUIPanelUpdate(CefRefPtr<CefRequest> request);
	CefRefPtr<CefResourceHandler> handleUIPanelClear(CefRefPtr<CefRequest> request);
	CefRefPtr<CefResourceHandler> serveLocalFile(const std::string& filePath);
	std::string getMimeType(const std::string& filePath) const;
	std::string loadFileContent(const std::string& filePath);

	std::string base_dir_;
	std::map<std::string, std::string> resource_cache_;
	
	IMPLEMENT_REFCOUNTING(LocalResourceRequestHandler);
};

/**
 * @brief Main resource loader class (facade for future expansion)
 * 
 * Currently wraps SimpleResourceHandler and LocalResourceRequestHandler.
 * Can be extended with caching, resource preloading, etc.
 */
class ResourcesLoader
{
public:
	ResourcesLoader() = default;
	~ResourcesLoader() = default;

	/**
	 * @brief Creates a resource request handler for CEF
	 * 
	 * @return CefRefPtr to LocalResourceRequestHandler
	 */
	static CefRefPtr<CefResourceRequestHandler> createRequestHandler();
};
