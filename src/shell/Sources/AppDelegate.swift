import AppKit

final class AppDelegate: NSObject, NSApplicationDelegate {
    private let pads = PadInput()
    private var renderer: Renderer?    // MTKView holds its delegate weakly, so keep it here
    private var window: NSWindow?
#if JOJO_HOST
    private var hostAudio: HostAudio?
#endif

    /// Starts the in-process runtime once a game folder is known (JOJO_HOST builds only).
    private func startHostIfReady() {
#if JOJO_HOST
        guard !GameHost.started, let folder = GameFolder.current else { return }
        GameHost.start(gameFolder: folder, settings: HostSettings.fromEnvironment())
        let audio = HostAudio()
        audio.start()
        hostAudio = audio
#endif
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        buildMenu()
        pads.start()

        guard let made = GameViewFactory.make(pads: pads) else {
            showFatal("This Mac has no Metal device, so JoJoShell cannot start.")
            return
        }
        let view = made.view
        self.renderer = made.renderer

        let window = NSWindow(
            contentRect: view.frame,
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.contentView = view
        window.contentAspectRatio = NSSize(width: 4, height: 3)
        window.center()
        window.makeKeyAndOrderFront(nil)
        window.makeFirstResponder(view)
        self.window = window
        NSApp.activate(ignoringOtherApps: true)

        updateTitle()
        startHostIfReady()
        if GameFolder.current == nil {
            // First launch (or the stored folder has gone): ask for the extracted game folder.
            DispatchQueue.main.async { [weak self] in
                self?.chooseGameFolder(firstLaunch: true)
            }
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }

    @objc func changeGameFolder(_ sender: Any?) {
        chooseGameFolder(firstLaunch: false)
    }

    private func chooseGameFolder(firstLaunch: Bool) {
        GameFolder.choose(in: window) { [weak self] url in
            guard let self = self, let url = url else { return }
            if GameFolder.remember(url) {
                self.updateTitle()
                self.startHostIfReady()
            } else {
                let alert = NSAlert()
                alert.messageText = "That folder has no \(GameFolder.indexFileName)."
                alert.informativeText = "Choose the folder that extract_disc.py wrote. It must contain \(GameFolder.indexFileName)."
                alert.addButton(withTitle: "OK")
                alert.runModal()
            }
        }
    }

    private func updateTitle() {
        if let folder = GameFolder.current {
            window?.title = "JoJo: \(folder.lastPathComponent)"
        } else {
            window?.title = "JoJo (no game folder chosen)"
        }
    }

    private func buildMenu() {
        let mainMenu = NSMenu()

        let appItem = NSMenuItem()
        mainMenu.addItem(appItem)
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "Quit JoJoShell", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appItem.submenu = appMenu

        let gameItem = NSMenuItem()
        mainMenu.addItem(gameItem)
        let gameMenu = NSMenu(title: "Game")
        let change = NSMenuItem(title: "Change Game Folder...", action: #selector(changeGameFolder(_:)), keyEquivalent: "o")
        change.target = self
        gameMenu.addItem(change)
        gameItem.submenu = gameMenu

        NSApp.mainMenu = mainMenu
    }

    private func showFatal(_ message: String) {
        let alert = NSAlert()
        alert.messageText = "JoJoShell cannot start"
        alert.informativeText = message
        alert.addButton(withTitle: "Quit")
        alert.runModal()
        NSApp.terminate(nil)
    }
}
