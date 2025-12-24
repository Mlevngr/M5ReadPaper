/**
 * Version Update Notifier
 * 检测版本更新并显示优雅的通知弹窗
 * 版本号自动从 manifest.json 读取，无需手动同步
 */

(function() {
  'use strict';

  const STORAGE_KEY = 'readpaper_last_version';
  let CURRENT_VERSION = '1.0.2'; // 默认值，将从 manifest 读取

  /**
   * 从 manifest.json 获取当前版本号
   */
  async function fetchCurrentVersion() {
    try {
      // 尝试从 Chrome Extension API 获取
      if (typeof chrome !== 'undefined' && chrome.runtime && chrome.runtime.getManifest) {
        const manifest = chrome.runtime.getManifest();
        return manifest.version;
      }
      
      // 如果不是扩展环境，尝试直接读取 manifest.json
      const response = await fetch('../manifest.json');
      if (response.ok) {
        const manifest = await response.json();
        return manifest.version;
      }
    } catch (error) {
      console.warn('[VersionNotifier] 无法读取 manifest 版本，使用默认值:', error);
    }
    return CURRENT_VERSION; // 返回默认值
  }

  /**
   * 获取上次记录的版本号
   */
  function getLastVersion() {
    return localStorage.getItem(STORAGE_KEY);
  }

  /**
   * 保存当前版本号
   */
  function saveCurrentVersion() {
    localStorage.setItem(STORAGE_KEY, CURRENT_VERSION);
  }

  /**
   * 比较版本号
   * @returns {boolean} true 表示当前版本更新
   */
  function isVersionUpdated(lastVersion) {
    if (!lastVersion) return true; // 首次使用
    
    const parseLast = lastVersion.split('.').map(Number);
    const parseCurrent = CURRENT_VERSION.split('.').map(Number);
    
    for (let i = 0; i < Math.max(parseLast.length, parseCurrent.length); i++) {
      const last = parseLast[i] || 0;
      const current = parseCurrent[i] || 0;
      if (current > last) return true;
      if (current < last) return false;
    }
    return false;
  }

  /**
   * 创建版本更新通知 DOM
   */
  function createUpdateNotification() {
    const overlay = document.createElement('div');
    overlay.className = 'version-update-overlay';
    overlay.innerHTML = `
      <div class="version-update-modal">
        <div class="version-update-header">
          <h2>🎉 版本更新</h2>
        </div>
        <div class="version-update-body">
          <div class="version-badge">
            <span class="version-label">新版本</span>
            <span class="version-number">v${CURRENT_VERSION}</span>
          </div>
          <div class="update-content">
            <h3>更新内容</h3>
            <ul class="update-list">
              <li>✨ 目录显示优化：自动定位到当前阅读章节</li>
              <li>⚡ 性能提升：目录缓存预加载，消除打开延迟</li>
              <li>🔧 修复若干已知问题</li>
            </ul>
          </div>
          <p class="update-tip">感谢您使用 ReadPaper！</p>
        </div>
        <div class="version-update-footer">
          <button class="button primary" id="btnCloseUpdateNotice">知道了</button>
        </div>
      </div>
    `;
    
    return overlay;
  }

  /**
   * 显示版本更新通知
   */
  function showUpdateNotification() {
    const notification = createUpdateNotification();
    document.body.appendChild(notification);
    
    // 添加显示动画
    requestAnimationFrame(() => {
      notification.classList.add('show');
    });
    
    // 关闭按钮事件
    const closeBtn = notification.querySelector('#btnCloseUpdateNotice');
    closeBtn.addEventListener('click', () => {
      notification.classList.remove('show');
      setTimeout(() => {
        notification.remove();
      }, 300);
      saveCurrentVersion();
    });
    
    // 点击遮罩层关闭
    notification.addEventListener('click', (e) => {
      if (e.target === notification) {
        closeBtn.click();
      }
    });
  }

  /**
   * 检查并显示版本更新通知
   */
  async function checkAndNotify() {
    // 先获取当前版本
    CURRENT_VERSION = await fetchCurrentVersion();
    
    const lastVersion = getLastVersion();
    
    if (isVersionUpdated(lastVersion)) {
      // 延迟显示，确保页面加载完成
      setTimeout(() => {
        showUpdateNotification();
      }, 500);
    }
  }

  /**
   * 初始化版本通知器
   */
  function init() {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', () => {
        checkAndNotify().catch(err => {
          console.error('[VersionNotifier] 初始化失败:', err);
        });
      });
    } else {
      checkAndNotify().catch(err => {
        console.error('[VersionNotifier] 初始化失败:', err);
      });
    }
  }

  // 暴露给全局，方便调试或手动触发
  window.VersionNotifier = {
    check: checkAndNotify,
    show: showUpdateNotification,
    reset: () => localStorage.removeItem(STORAGE_KEY),
    getCurrentVersion: async () => {
      if (CURRENT_VERSION === '1.0.2') {
        // 如果还是默认值，尝试重新获取
        CURRENT_VERSION = await fetchCurrentVersion();
      }
      return CURRENT_VERSION;
    },
    getLastVersion: getLastVersion
  };

  // 自动初始化
  init();
})();
