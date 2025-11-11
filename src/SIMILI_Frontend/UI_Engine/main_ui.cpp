#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_sandbox_win.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "ui_app.hpp"
#include "ui_handler.hpp"
#include "simple_window_delegate.hpp"
#include "simple_browser_view_delegate.hpp"
#include "../../SIMILI_Services/router/RouterSim.hpp"
#include "../../SIMILI_Services/types/RouterTypes.hpp"
#include "../../SIMILI_Services/middleware/SimpleHttpServer.hpp"
#include "../../Engine/OpenGLContext.hpp"
#include "../../Engine/ThreeDScene.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../WorldObjects/Mesh/Mesh.hpp"
#include "../../Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include <iostream>
#include <sstream>
#include <glm/glm.hpp>
#include <filesystem>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace fs = std::filesystem;
fs::path gExecutableDir;

std::string httpGet(const std::string& host, const std::string& port, const std::string& target) 
{
	namespace beast = boost::beast;
	namespace http = beast::http;
	namespace net = boost::asio;
	using tcp = net::ip::tcp;
	
	try 
	{
		net::io_context ioc;
		tcp::resolver resolver(ioc);
		beast::tcp_stream stream(ioc);
		
		auto const results = resolver.resolve(host, port);
		stream.connect(results);
		
		http::request<http::string_body> req{http::verb::get, target, 11};
		req.set(http::field::host, host);
		req.set(http::field::user_agent, "SIMILI_UI/1.0");
		
		http::write(stream, req);
		
		beast::flat_buffer buffer;
		http::response<http::string_body> res;
		http::read(stream, buffer, res);
		
		beast::error_code ec;
		stream.socket().shutdown(tcp::socket::shutdown_both, ec);
		
		if (res.result() == http::status::ok) {
			return res.body();
		}
		return "";
	}
	catch (const std::exception& e) 
	{
		std::cerr << "[httpGet] Error: " << e.what() << std::endl;
		return "";
	}
}

#ifdef _WIN32
#include <windows.h>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) 
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	CefMainArgs main_args(hInstance);
	
	CefRefPtr<UIApp> app(new UIApp);

	int exit_code = CefExecuteProcess(main_args, app, nullptr);
	if (exit_code >= 0) 
	{
		return exit_code;
	}

	static bool console_allocated = false;
	if (!console_allocated) 
	{
		AllocConsole();
		FILE* fp;
		freopen_s(&fp, "CONOUT$", "w", stdout);
		freopen_s(&fp, "CONOUT$", "w", stderr);
		console_allocated = true;
	}

	{
		wchar_t exePath[MAX_PATH];
		GetModuleFileNameW(NULL, exePath, MAX_PATH);
		gExecutableDir = fs::path(exePath).parent_path();
	}

	std::cout << "[Main] About to start HTTP Server..." << std::endl;
	
	// Declare shared variables BEFORE creating routes so lambdas can capture them
	static std::string sharedContextID;  // Static to capture in lambda
	static std::string sharedSceneID;    // Static to capture in lambda
	
	// Start HTTP/HTTPS Server (singleton - will only start once)
	// UI process uses port 8081 to avoid conflict with main process (8080)
	try {
		SIMILI::Server::SimpleHttpServer::getInstance().start(8081, 8443);
		std::cout << "[Main_UI] HTTP Server started on port 8081" << std::endl;
	} 
	catch (const std::exception& e) 
	{
		std::cerr << "[Main_UI] ERROR starting HTTP Server: " << e.what() << std::endl;
		return -1;
	}

	// Get router from the singleton server and add routes
	SIMILI::Router::RouterSim& router = SIMILI::Server::SimpleHttpServer::getInstance().getRouter();
	router.setDebugMode(true);
	
	router.get("/ping", [](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response 
	{

		SIMILI::Router::Response response(msg.requestId);
		response.body = R"({"message":"pong from UI"})";
		return response;
	}, "Connectivity test from UI");
	
	router.get("/api/scene-info", [](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response 
	{
		SIMILI::Router::Response response(msg.requestId);
		std::ostringstream json;
		json << "{"
		     << "\"sceneID\": \"" << sharedSceneID << "\","
		     << "\"contextID\": \"" << sharedContextID << "\""
		     << "}";
		response.body = json.str();
		response.headers["Content-Type"] = "application/json";
		response.headers["Access-Control-Allow-Origin"] = "*";
		return response;
	}, "Get current scene info");

	std::cout << "[Main] Router SIMILI Initialized" << std::endl;

	// ===== Retrieve shared 3D Scene and OpenGL Context info via HTTP =====
	
	std::cout << "[Main_UI] Requesting shared context from main process via HTTP..." << std::endl;
	
	ThreeDScene* myThreeDScene = nullptr;
	bool contextRetrieved = false;
	
	try 
	{
		std::string jsonResponse = httpGet("localhost", "8080", "/api/context");
		
		if (!jsonResponse.empty()) {
			size_t contextPos = jsonResponse.find("\"contextID\": \"");
			size_t scenePos = jsonResponse.find("\"sceneID\": \"");
			
			if (contextPos != std::string::npos && scenePos != std::string::npos) 
			{
				contextPos += 14;
				size_t contextEnd = jsonResponse.find("\"", contextPos);
				sharedContextID = jsonResponse.substr(contextPos, contextEnd - contextPos);
				
				scenePos += 12;
				size_t sceneEnd = jsonResponse.find("\"", scenePos);
				sharedSceneID = jsonResponse.substr(scenePos, sceneEnd - scenePos);
				
				contextRetrieved = true;
			}
		}
	} catch (const std::exception& e) {
		std::cerr << "[Main_UI] HTTP request failed: " << e.what() << std::endl;
		contextRetrieved = false;
	}
	
	if (contextRetrieved) 
	{
		myThreeDScene = new ThreeDScene();
	} 
	else 
	{
		std::cerr << "[Main_UI] Could not retrieve main process context via HTTP" << std::endl;
		std::cerr << "[Main_UI] Creating independent scene..." << std::endl;
		myThreeDScene = new ThreeDScene();
	}
	
	static Camera* mainCamera = new Camera();
	static Mesh* cubeMesh1 = nullptr;
	
	mainCamera->setName("MainCamera");
		
	CefSettings settings;
	settings.no_sandbox = true;
	settings.multi_threaded_message_loop = false;
	
	// Disable logging to avoid errors
	settings.log_severity = LOGSEVERITY_DISABLE;

	if (!CefInitialize(main_args, settings, app, nullptr)) {
		std::cerr << "[Main] Failed to initialize CEF" << std::endl;
		return -1;
	}

	CefRefPtr<UIHandler> handler(new UIHandler);

	handler->setThreeDScene(myThreeDScene);
	std::cout << "[Main_UI] 3D Scene linked to UIHandler" << std::endl;
	
	handler->setSceneObjects(nullptr, myThreeDScene, mainCamera, &cubeMesh1);
	std::cout << "[Main_UI] Scene objects (camera, mesh) passed to UIHandler" << std::endl;
	
	CefBrowserSettings browser_settings;
	browser_settings.windowless_frame_rate = 60;

	std::string url = "file:///ui/main_layout.html";

	CefRefPtr<SimpleBrowserViewDelegate> browser_view_delegate(new SimpleBrowserViewDelegate());
	
	CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
		handler, url, browser_settings, nullptr, nullptr, browser_view_delegate);

	CefRefPtr<SimpleWindowDelegate> window_delegate(new SimpleWindowDelegate(browser_view));
	
	CefWindow::CreateTopLevelWindow(window_delegate);

	CefRunMessageLoop();

	CefShutdown();

	return 0;
}
#endif
 
