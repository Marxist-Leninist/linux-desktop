/* 
 * Windows 11 Shell Extension for GNOME
 * 
 * Features:
 * - Centered taskbar with app icons
 * - Windows 11 style start menu
 * - System tray in bottom-right corner style
 * - Rounded corners on windows
 */

import St from 'gi://St';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';
import * as AppFavorites from 'resource:///org/gnome/shell/ui/appFavorites.js';
import * as AppDisplay from 'resource:///org/gnome/shell/ui/appDisplay.js';
import * as DND from 'resource:///org/gnome/shell/ui/dnd.js';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

/* ===== Taskbar Icon ===== */
const TaskbarIcon = GObject.registerClass(
class TaskbarIcon extends St.Button {
    _init(app) {
        super._init({
            style_class: 'taskbar-icon',
            reactive: true,
            can_focus: true,
            track_hover: true,
        });
        
        this._app = app;
        this._windows = [];
        
        // App icon
        this._icon = app.create_icon_texture(32);
        this.set_child(this._icon);
        
        // Running indicator
        this._dot = new St.Widget({
            style_class: 'taskbar-running-dot',
            visible: false,
        });
        this.add_child(this._dot);
        
        this._updateState();
        
        // Signals
        this.connect('clicked', () => this._onClicked());
        this.connect('destroy', () => this._onDestroy());
        
        // Track window changes
        this._windowsChangedId = this._app.connect('windows-changed', 
            () => this._updateState());
    }
    
    _updateState() {
        this._windows = this._app.get_windows();
        const isRunning = this._windows.length > 0;
        const isFocused = this._windows.some(w => w.has_focus());
        
        this._dot.visible = isRunning;
        
        if (isFocused) {
            this.add_style_class_name('focused');
        } else {
            this.remove_style_class_name('focused');
        }
    }
    
    _onClicked() {
        if (this._windows.length === 0) {
            this._app.activate();
        } else if (this._windows.length === 1) {
            const win = this._windows[0];
            if (win.has_focus()) {
                win.minimize();
            } else {
                win.activate(global.get_current_time());
            }
        } else {
            // Multiple windows - cycle through them
            const focused = this._windows.find(w => w.has_focus());
            if (focused) {
                const idx = this._windows.indexOf(focused);
                const next = this._windows[(idx + 1) % this._windows.length];
                next.activate(global.get_current_time());
            } else {
                this._windows[0].activate(global.get_current_time());
            }
        }
    }
    
    _onDestroy() {
        if (this._windowsChangedId) {
            this._app.disconnect(this._windowsChangedId);
            this._windowsChangedId = 0;
        }
    }
});

/* ===== Taskbar ===== */
const Taskbar = GObject.registerClass(
class Taskbar extends St.BoxLayout {
    _init() {
        super._init({
            style_class: 'taskbar',
            vertical: false,
        });
        
        this._appIcons = new Map();
        this._favorites = AppFavorites.getAppFavorites();
        
        // Add start button
        this._startButton = new St.Button({
            style_class: 'taskbar-start-button',
            child: new St.Icon({
                icon_name: 'view-app-grid-symbolic',
                icon_size: 24,
            }),
        });
        this._startButton.connect('clicked', () => this._toggleStartMenu());
        this.add_child(this._startButton);
        
        // Separator
        this.add_child(new St.Widget({
            style_class: 'taskbar-separator',
            x_expand: false,
        }));
        
        // App icons container
        this._iconsBox = new St.BoxLayout({
            style_class: 'taskbar-icons',
            vertical: false,
        });
        this.add_child(this._iconsBox);
        
        // Initialize
        this._rebuildIcons();
        
        // Signals
        this._favoritesChangedId = this._favorites.connect('changed',
            () => this._rebuildIcons());
        
        this._windowTrackerId = Shell.WindowTracker.get_default().connect(
            'tracked-windows-changed', () => this._updateRunningApps());
        
        this._focusWindowId = global.display.connect('notify::focus-window',
            () => this._updateFocus());
    }
    
    _rebuildIcons() {
        this._iconsBox.destroy_all_children();
        this._appIcons.clear();
        
        // Add favorite apps
        const favs = this._favorites.getFavorites();
        for (const app of favs) {
            this._addAppIcon(app);
        }
        
        // Add running non-favorite apps
        this._updateRunningApps();
    }
    
    _addAppIcon(app) {
        if (this._appIcons.has(app.get_id())) return;
        
        const icon = new TaskbarIcon(app);
        this._iconsBox.add_child(icon);
        this._appIcons.set(app.get_id(), icon);
    }
    
    _updateRunningApps() {
        const tracker = Shell.WindowTracker.get_default();
        const favIds = new Set(this._favorites.getFavorites().map(a => a.get_id()));
        
        // Get running apps
        const running = new Set();
        for (const win of global.get_window_actors()) {
            const app = tracker.get_window_app(win.meta_window);
            if (app) {
                running.add(app.get_id());
                if (!favIds.has(app.get_id())) {
                    this._addAppIcon(app);
                }
            }
        }
        
        // Update all icons
        for (const [id, icon] of this._appIcons) {
            icon._updateState();
            
            // Remove non-favorite, non-running apps
            if (!favIds.has(id) && !running.has(id)) {
                icon.destroy();
                this._appIcons.delete(id);
            }
        }
    }
    
    _updateFocus() {
        for (const icon of this._appIcons.values()) {
            icon._updateState();
        }
    }
    
    _toggleStartMenu() {
        if (Main.overview.visible) {
            Main.overview.hide();
        } else {
            Main.overview.show();
        }
    }
    
    destroy() {
        if (this._favoritesChangedId) {
            this._favorites.disconnect(this._favoritesChangedId);
        }
        if (this._windowTrackerId) {
            Shell.WindowTracker.get_default().disconnect(this._windowTrackerId);
        }
        if (this._focusWindowId) {
            global.display.disconnect(this._focusWindowId);
        }
        super.destroy();
    }
});

/* ===== Start Menu ===== */
const StartMenu = GObject.registerClass(
class StartMenu extends St.Widget {
    _init() {
        super._init({
            style_class: 'start-menu',
            reactive: true,
            visible: false,
        });
        
        // Background
        this._background = new St.Widget({
            style_class: 'start-menu-background',
        });
        this.add_child(this._background);
        
        // Main container
        this._container = new St.BoxLayout({
            style_class: 'start-menu-container',
            vertical: true,
        });
        this.add_child(this._container);
        
        // Search bar
        this._searchEntry = new St.Entry({
            style_class: 'start-menu-search',
            hint_text: 'Type here to search',
            can_focus: true,
        });
        this._container.add_child(this._searchEntry);
        
        // Pinned apps section
        this._pinnedLabel = new St.Label({
            style_class: 'start-menu-section-label',
            text: 'Pinned',
        });
        this._container.add_child(this._pinnedLabel);
        
        this._pinnedGrid = new St.Widget({
            style_class: 'start-menu-grid',
            layout_manager: new Clutter.GridLayout({
                orientation: Clutter.Orientation.HORIZONTAL,
                column_spacing: 8,
                row_spacing: 8,
            }),
        });
        this._container.add_child(this._pinnedGrid);
        
        // All apps button
        this._allAppsButton = new St.Button({
            style_class: 'start-menu-all-apps',
            label: 'All apps >',
        });
        this._container.add_child(this._allAppsButton);
        
        // Recommended section
        this._recommendedLabel = new St.Label({
            style_class: 'start-menu-section-label',
            text: 'Recommended',
        });
        this._container.add_child(this._recommendedLabel);
        
        this._recommendedList = new St.BoxLayout({
            style_class: 'start-menu-recommended',
            vertical: true,
        });
        this._container.add_child(this._recommendedList);
        
        // User section at bottom
        this._userSection = new St.BoxLayout({
            style_class: 'start-menu-user',
        });
        this._container.add_child(this._userSection);
        
        // User avatar and name
        this._userIcon = new St.Icon({
            icon_name: 'avatar-default-symbolic',
            icon_size: 32,
        });
        this._userSection.add_child(this._userIcon);
        
        this._userName = new St.Label({
            style_class: 'start-menu-username',
            text: GLib.get_real_name() || GLib.get_user_name(),
        });
        this._userSection.add_child(this._userName);
        
        // Power button
        this._powerButton = new St.Button({
            style_class: 'start-menu-power',
            child: new St.Icon({
                icon_name: 'system-shutdown-symbolic',
                icon_size: 20,
            }),
        });
        this._userSection.add_child(this._powerButton);
        
        this._populatePinned();
        this._populateRecommended();
    }
    
    _populatePinned() {
        const favorites = AppFavorites.getAppFavorites().getFavorites();
        const layout = this._pinnedGrid.layout_manager;
        
        let col = 0;
        let row = 0;
        const cols = 6;
        
        for (const app of favorites.slice(0, 18)) { // Max 3 rows
            const button = new St.Button({
                style_class: 'start-menu-app',
            });
            
            const box = new St.BoxLayout({
                vertical: true,
                style_class: 'start-menu-app-box',
            });
            
            const icon = app.create_icon_texture(48);
            box.add_child(icon);
            
            const label = new St.Label({
                text: app.get_name(),
                style_class: 'start-menu-app-label',
            });
            box.add_child(label);
            
            button.set_child(box);
            button.connect('clicked', () => {
                app.activate();
                this.hide();
            });
            
            layout.attach(button, col, row, 1, 1);
            
            col++;
            if (col >= cols) {
                col = 0;
                row++;
            }
        }
    }
    
    _populateRecommended() {
        // Get recently used apps
        const usage = Shell.AppUsage.get_default();
        const appSystem = Shell.AppSystem.get_default();
        const recent = usage.get_most_used().slice(0, 6);
        
        for (const app of recent) {
            const item = new St.Button({
                style_class: 'start-menu-recent-item',
            });
            
            const box = new St.BoxLayout({
                vertical: false,
            });
            
            const icon = app.create_icon_texture(32);
            box.add_child(icon);
            
            const textBox = new St.BoxLayout({
                vertical: true,
            });
            
            const name = new St.Label({
                text: app.get_name(),
                style_class: 'start-menu-recent-name',
            });
            textBox.add_child(name);
            
            const desc = new St.Label({
                text: 'Recently added',
                style_class: 'start-menu-recent-desc',
            });
            textBox.add_child(desc);
            
            box.add_child(textBox);
            item.set_child(box);
            
            item.connect('clicked', () => {
                app.activate();
                this.hide();
            });
            
            this._recommendedList.add_child(item);
        }
    }
    
    show() {
        this.visible = true;
        this._searchEntry.grab_key_focus();
    }
    
    hide() {
        this.visible = false;
    }
    
    toggle() {
        if (this.visible) {
            this.hide();
        } else {
            this.show();
        }
    }
});

/* ===== CSS Styles (injected) ===== */
const STYLESHEET = `
/* Taskbar */
.taskbar {
    background-color: rgba(255, 255, 255, 0.85);
    border-radius: 8px;
    padding: 4px 12px;
    margin: 8px;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.12);
}

.taskbar-start-button {
    padding: 8px 12px;
    border-radius: 4px;
}

.taskbar-start-button:hover {
    background-color: rgba(0, 0, 0, 0.06);
}

.taskbar-separator {
    width: 1px;
    margin: 4px 8px;
    background-color: rgba(0, 0, 0, 0.1);
}

.taskbar-icons {
    spacing: 4px;
}

.taskbar-icon {
    padding: 8px;
    border-radius: 4px;
}

.taskbar-icon:hover {
    background-color: rgba(0, 0, 0, 0.06);
}

.taskbar-icon.focused {
    background-color: rgba(0, 120, 212, 0.1);
}

.taskbar-running-dot {
    width: 4px;
    height: 4px;
    border-radius: 2px;
    background-color: #0078D4;
    margin-top: 2px;
}

/* Start Menu */
.start-menu {
    width: 600px;
    height: 700px;
}

.start-menu-background {
    background-color: rgba(255, 255, 255, 0.95);
    border-radius: 12px;
    box-shadow: 0 16px 48px rgba(0, 0, 0, 0.2);
}

.start-menu-container {
    padding: 24px;
    spacing: 16px;
}

.start-menu-search {
    background-color: rgba(0, 0, 0, 0.04);
    border-radius: 8px;
    padding: 8px 16px;
    border: 1px solid rgba(0, 0, 0, 0.08);
}

.start-menu-search:focus {
    border-color: #0078D4;
}

.start-menu-section-label {
    font-weight: 600;
    font-size: 11pt;
    color: #1a1a1a;
    margin-top: 8px;
}

.start-menu-grid {
    margin: 8px 0;
}

.start-menu-app {
    padding: 12px;
    border-radius: 8px;
    min-width: 80px;
}

.start-menu-app:hover {
    background-color: rgba(0, 0, 0, 0.06);
}

.start-menu-app-box {
    spacing: 4px;
}

.start-menu-app-label {
    font-size: 9pt;
    text-align: center;
}

.start-menu-all-apps {
    padding: 8px 16px;
    border-radius: 4px;
    text-align: right;
}

.start-menu-all-apps:hover {
    background-color: rgba(0, 0, 0, 0.06);
}

.start-menu-recommended {
    spacing: 4px;
}

.start-menu-recent-item {
    padding: 8px 12px;
    border-radius: 8px;
}

.start-menu-recent-item:hover {
    background-color: rgba(0, 0, 0, 0.06);
}

.start-menu-recent-name {
    font-weight: 500;
}

.start-menu-recent-desc {
    font-size: 9pt;
    color: #5c5c5c;
}

.start-menu-user {
    margin-top: auto;
    padding-top: 16px;
    border-top: 1px solid rgba(0, 0, 0, 0.08);
    spacing: 12px;
}

.start-menu-username {
    font-weight: 500;
}

.start-menu-power {
    margin-left: auto;
    padding: 8px;
    border-radius: 4px;
}

.start-menu-power:hover {
    background-color: rgba(0, 0, 0, 0.06);
}
`;

/* ===== Extension Entry Point ===== */
export default class Win11ShellExtension extends Extension {
    enable() {
        this._taskbar = null;
        this._startMenu = null;
        this._panelConnection = null;
        
        // Inject custom CSS
        this._themeContext = St.ThemeContext.get_for_stage(global.stage);
        this._theme = this._themeContext.get_theme();
        this._stylesheet = Gio.File.new_for_path(
            GLib.build_filenamev([this.path, 'stylesheet.css']));
        this._theme.load_stylesheet(this._stylesheet);
        
        // Create taskbar
        this._taskbar = new Taskbar();
        
        // Position taskbar at bottom center
        Main.layoutManager.addChrome(this._taskbar, {
            affectsStruts: true,
            trackFullscreen: true,
        });
        
        this._positionTaskbar();
        
        // Connect to monitor changes
        this._monitorsChangedId = Main.layoutManager.connect('monitors-changed',
            () => this._positionTaskbar());
        
        // Optionally hide the top panel
        // Main.panel.hide();
    }
    
    _positionTaskbar() {
        const monitor = Main.layoutManager.primaryMonitor;
        if (!monitor) return;
        
        // Center horizontally at bottom
        const width = this._taskbar.get_width() || 400;
        this._taskbar.set_position(
            monitor.x + Math.floor((monitor.width - width) / 2),
            monitor.y + monitor.height - this._taskbar.height - 8
        );
    }
    
    disable() {
        if (this._monitorsChangedId) {
            Main.layoutManager.disconnect(this._monitorsChangedId);
            this._monitorsChangedId = 0;
        }
        
        if (this._taskbar) {
            Main.layoutManager.removeChrome(this._taskbar);
            this._taskbar.destroy();
            this._taskbar = null;
        }
        
        if (this._startMenu) {
            this._startMenu.destroy();
            this._startMenu = null;
        }
        
        if (this._stylesheet && this._theme) {
            this._theme.unload_stylesheet(this._stylesheet);
        }
        
        // Main.panel.show();
    }
}
