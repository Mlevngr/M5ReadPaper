#pragma once

#include <WebServer.h>

class WiFiHotspotManager;

/**
 * ApiRouter
 * 负责将对外提供的 HTTP API 端点挂载到给定的 WebServer。
 * 这些端点覆盖 template.html 目前使用的功能：
 * - 列表：/list, /list/book, /list/font, /list/image (JSON 流式响应)
 * - 上传：/upload (GET 引导、POST 实际上传)
 * - 删除：/delete (JSON)
 * - 下载：/download (文件流)
 * - 同步时间：/sync_time (POST JSON)
 * 说明：端点路径保持兼容，便于前端复用；实现细节委托给 WiFiHotspotManager 现有私有处理函数。
 */
class ApiRouter {
public:
    static void registerRoutes(WebServer& server, WiFiHotspotManager& mgr);
};
