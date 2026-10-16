import AppKit

/// The user's extracted game folder. It is the folder that extract_disc.py wrote, identified by its index.tsv.
/// The path is stored in UserDefaults under `gameFolderPath`. The app is not sandboxed, so a plain path is enough.
/// Passing `-gameFolderPath <dir>` on the command line overrides the stored value (used by smoke-test.sh).
enum GameFolder {
    static let defaultsKey = "gameFolderPath"
    static let indexFileName = "index.tsv"

    static var current: URL? {
        guard let path = UserDefaults.standard.string(forKey: defaultsKey), !path.isEmpty else { return nil }
        let url = URL(fileURLWithPath: path, isDirectory: true)
        return isValid(url) ? url : nil
    }

    static func isValid(_ url: URL) -> Bool {
        FileManager.default.fileExists(atPath: url.appendingPathComponent(indexFileName).path)
    }

    /// Shows an open panel as a sheet on `window` (or as a free panel when there is no window).
    /// Calls `completion` with the chosen folder, or nil when the user cancels.
    static func choose(in window: NSWindow?, completion: @escaping (URL?) -> Void) {
        let panel = NSOpenPanel()
        panel.title = "Choose the JoJo game folder"
        panel.message = "Pick the folder made by extract_disc.py (it contains index.tsv)."
        panel.prompt = "Choose"
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.canCreateDirectories = false
        if let current = current {
            panel.directoryURL = current
        }

        let handle: (NSApplication.ModalResponse) -> Void = { response in
            completion(response == .OK ? panel.url : nil)
        }
        if let window = window {
            panel.beginSheetModal(for: window, completionHandler: handle)
        } else {
            panel.begin(completionHandler: handle)
        }
    }

    /// Stores `url` if it holds an index.tsv. Returns false (and stores nothing) otherwise.
    @discardableResult
    static func remember(_ url: URL) -> Bool {
        guard isValid(url) else { return false }
        UserDefaults.standard.set(url.path, forKey: defaultsKey)
        return true
    }
}
