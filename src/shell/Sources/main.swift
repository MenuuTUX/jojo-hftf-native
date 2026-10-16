import AppKit

// Entry point. The window, menu and game loop are set up in AppDelegate.
let app = NSApplication.shared
let appDelegate = AppDelegate()
app.delegate = appDelegate
app.setActivationPolicy(.regular)
app.run()
