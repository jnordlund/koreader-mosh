local BD = require("ui/bidi")
local DataStorage = require("datastorage")
local Device = require("device")
local InfoMessage = require("ui/widget/infomessage")
local UIManager = require("ui/uimanager")
local WidgetContainer = require("ui/widget/container/widgetcontainer")
local ffiUtil = require("ffi/util")
local lfs = require("libs/libkoreader-lfs")
local _ = require("gettext")
local T = ffiUtil.template

local SHIM_MARKER = "# Managed by koreader-mosh"

local Mosh = WidgetContainer:extend{
    name = "mosh",
}

local function joinPath(...)
    return ffiUtil.joinPath(...)
end

local function pluginDir()
    local source = debug.getinfo(1, "S").source:gsub("^@", "")
    return source:match("(.+)/main%.lua$") or "./plugins/mosh.koplugin"
end

local function koreaderDir()
    local dir = pluginDir()
    return dir:match("^(.*)/plugins/mosh%.koplugin$") or "."
end

local function readFirstLine(path)
    local file = io.open(path, "r")
    if not file then return nil end
    local line = file:read("*l")
    file:close()
    return line
end

local function readAll(path)
    local file = io.open(path, "r")
    if not file then return nil end
    local text = file:read("*a")
    file:close()
    return text
end

local function writeAll(path, text)
    local file, err = io.open(path, "w")
    if not file then return nil, err end
    file:write(text)
    file:close()
    return true
end

local function exists(path)
    return lfs.attributes(path) ~= nil
end

local function isExecutable(path)
    return ffiUtil.isExecutable(path)
end

local function shellQuote(path)
    return "'" .. tostring(path):gsub("'", "'\\''") .. "'"
end

local function shimPath()
    return joinPath(DataStorage:getDataDir(), "scripts", "mosh")
end

local function shimText()
    return table.concat({
        "#!/bin/sh",
        SHIM_MARKER,
        "PLUGIN_DIR=" .. shellQuote(pluginDir()),
        'export TERM=vt52',
        'export TERMINFO="${PLUGIN_DIR}/share/terminfo"',
        'export TERMINFO_DIRS="${PLUGIN_DIR}/share/terminfo"',
        "export HOME=" .. shellQuote(koreaderDir()),
        'exec "${PLUGIN_DIR}/bin/mosh" "$@"',
        "",
    }, "\n")
end

local function shimState()
    local path = shimPath()
    local text = readAll(path)
    if not text then return "missing" end
    if text:find(SHIM_MARKER, 1, true) then
        if text == shimText() then
            return "current"
        end
        return "managed-outdated"
    end
    return "foreign"
end

function Mosh:isSupportedDevice()
    return Device:isKobo()
end

function Mosh:paths()
    local dir = pluginDir()
    local root = koreaderDir()
    return {
        plugin_dir = dir,
        koreader_dir = root,
        launcher = joinPath(dir, "bin", "mosh"),
        client = joinPath(dir, "bin", "mosh-client"),
        terminfo = joinPath(dir, "share", "terminfo", "v", "vt52"),
        terminfo_hex = joinPath(dir, "share", "terminfo", "76", "vt52"),
        dbclient = joinPath(root, "dbclient"),
        shim = shimPath(),
        scripts = joinPath(DataStorage:getDataDir(), "scripts"),
    }
end

function Mosh:statusText()
    local paths = self:paths()
    local version = readFirstLine(joinPath(paths.plugin_dir, "VERSION")) or "unknown"
    local supported = self:isSupportedDevice() and _("yes") or _("no")
    local lines = {
        T(_("Version: %1"), version),
        T(_("Kobo platform: %1"), supported),
        T(_("Plugin: %1"), BD.filepath(paths.plugin_dir)),
        T(_("Launcher: %1"), isExecutable(paths.launcher) and _("ok") or _("missing or not executable")),
        T(_("mosh-client: %1"), isExecutable(paths.client) and _("ok") or _("missing or not executable")),
        T(_("vt52 terminfo: %1"), (exists(paths.terminfo) or exists(paths.terminfo_hex)) and _("ok") or _("missing")),
        T(_("dbclient: %1"), isExecutable(paths.dbclient) and _("ok") or _("missing or not executable")),
        T(_("Terminal command: %1"), shimState()),
        _("Locale: launcher sets LANG/LC_CTYPE=C.UTF-8; the packaged client must provide a working UTF-8 libc or locale."),
    }
    return table.concat(lines, "\n")
end

function Mosh:installShim()
    local paths = self:paths()
    lfs.mkdir(paths.scripts)

    local state = shimState()
    if state == "foreign" then
        UIManager:show(InfoMessage:new{
            text = T(_("Refusing to overwrite an existing non-managed script:\n%1"), BD.filepath(paths.shim)),
        })
        return false
    end

    local tmp = paths.shim .. ".tmp"
    local ok, err = writeAll(tmp, shimText())
    if not ok then
        UIManager:show(InfoMessage:new{
            text = T(_("Could not write shim:\n%1"), err or paths.shim),
        })
        return false
    end
    os.execute("chmod 755 " .. shellQuote(tmp))
    if os.rename(tmp, paths.shim) ~= true then
        os.remove(tmp)
        UIManager:show(InfoMessage:new{
            text = T(_("Could not install shim:\n%1"), BD.filepath(paths.shim)),
        })
        return false
    end
    UIManager:show(InfoMessage:new{
        text = T(_("Installed terminal command:\n%1"), BD.filepath(paths.shim)),
    })
    return true
end

function Mosh:removeShim()
    local state = shimState()
    if state == "foreign" then
        UIManager:show(InfoMessage:new{
            text = T(_("Refusing to remove a non-managed script:\n%1"), BD.filepath(shimPath())),
        })
        return false
    end
    if state == "missing" then
        UIManager:show(InfoMessage:new{ text = _("No Mosh terminal command is installed.") })
        return true
    end
    os.remove(shimPath())
    UIManager:show(InfoMessage:new{ text = _("Removed Mosh terminal command.") })
    return true
end

function Mosh:showUsage()
    UIManager:show(InfoMessage:new{
        timeout = 20,
        text = _("Open KOReader's terminal and run:\nmosh user@example.com\n\nSupported options include -p, --ssh-port, --identity, --predict, --no-init, -4 and -6.\n\nFor sleep/wake resume, leave the terminal session running. Use the terminal's Close action to hide it; do not Quit the session, exit the shell, close KOReader, or reboot the device.\n\nIf startup fails, check the status screen for dbclient, terminfo, locale and binary availability. The remote host must have mosh-server installed and UDP reachable."),
    })
end

function Mosh:init()
    self.ui.menu:registerToMainMenu(self)
end

function Mosh:addToMainMenu(menu_items)
    menu_items.mosh_client = {
        text = _("Mosh client"),
        enabled_func = function()
            return self:isSupportedDevice()
        end,
        sub_item_table = {
            {
                text = _("Status"),
                keep_menu_open = true,
                callback = function()
                    UIManager:show(InfoMessage:new{
                        timeout = 20,
                        text = self:statusText(),
                    })
                end,
            },
            {
                text = _("Install or update terminal command"),
                keep_menu_open = true,
                callback = function(touchmenu_instance)
                    self:installShim()
                    if touchmenu_instance then touchmenu_instance:updateItems() end
                end,
            },
            {
                text = _("Remove terminal command"),
                keep_menu_open = true,
                enabled_func = function()
                    return shimState() ~= "missing"
                end,
                callback = function(touchmenu_instance)
                    self:removeShim()
                    if touchmenu_instance then touchmenu_instance:updateItems() end
                end,
            },
            {
                text = _("Usage and troubleshooting"),
                keep_menu_open = true,
                callback = function()
                    self:showUsage()
                end,
            },
        },
    }
end

return Mosh
