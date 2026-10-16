import AppKit
import GameController

/// PS1 digital button bits, matching the convention in platform/pad.h (bit set = released, active low).
enum PS1Button: UInt16 {
    case select = 0, l3, r3, start, up, right, down, left, l2, r2, l1, r1, triangle, circle, cross, square

    var mask: UInt16 { 1 << rawValue }
}

/// Reads up to two controllers and the keyboard once per frame and publishes them with plat_pad_set.
/// Main thread only (called from the MTKView draw callback).
final class PadInput {
    private static let released: UInt16 = 0xFFFF

    /// Keyboard fallback. Key codes are the Carbon kVK_ values, which NSEvent.keyCode reports.
    /// Face buttons are positional: Z = cross (bottom), X = circle (right), A = square (left), S = triangle (top).
    private static let keyMap: [UInt16: PS1Button] = [
        126: .up,            // up arrow
        125: .down,          // down arrow
        123: .left,          // left arrow
        124: .right,         // right arrow
        6: .cross,           // Z
        7: .circle,          // X
        0: .square,          // A
        1: .triangle,        // S
        12: .l1,             // Q
        13: .r1,             // W
        14: .l2,             // E
        15: .r2,             // R
        36: .start,          // Return
        76: .start,          // keypad Enter
    ]
    private static let rightShiftKeyCode: UInt16 = 60
    private static let deviceRightShiftMask: UInt = 0x0000_0004   // NX_DEVICERSHIFTKEYMASK

    private var slots: [GCController] = []
    private var keysDown = Set<UInt16>()
    private var rightShiftDown = false
    private var eventMonitor: Any?
    private var observers: [NSObjectProtocol] = []

    func start() {
        refreshSlots()
        let center = NotificationCenter.default
        observers.append(center.addObserver(forName: .GCControllerDidConnect, object: nil, queue: .main) { [weak self] _ in
            self?.refreshSlots()
        })
        observers.append(center.addObserver(forName: .GCControllerDidDisconnect, object: nil, queue: .main) { [weak self] _ in
            self?.refreshSlots()
        })
        observers.append(center.addObserver(forName: NSApplication.didResignActiveNotification, object: nil, queue: .main) { [weak self] _ in
            self?.clearKeyboard()
        })
        eventMonitor = NSEvent.addLocalMonitorForEvents(matching: [.keyDown, .keyUp, .flagsChanged]) { [weak self] event in
            self?.handle(event)
            return event
        }
    }

    /// Publishes port 0 (controller 1 ANDed with keyboard) and port 1 (controller 2) for this frame.
    /// Both ports are sent every frame. An absent controller is reported as disconnected.
    func pushFrame() {
        let keyboard = keyboardWord()

        var port0 = PlatPad()
        port0.connected = 1          // the keyboard is always available as port 0
        port0.buttons = keyboard & (slots.first.map(readController) ?? PadInput.released)
        port0.lx = 0x80; port0.ly = 0x80; port0.rx = 0x80; port0.ry = 0x80
        port0.analog = 0             // digital pad only, per the requirements
        plat_pad_set(0, &port0)
#if JOJO_HOST
        jojo_host_set_pad(0, port0.buttons, 1)
#endif

        var port1 = PlatPad()
        if slots.count > 1 {
            port1.connected = 1
            port1.buttons = readController(slots[1])
        } else {
            port1.connected = 0
            port1.buttons = PadInput.released
        }
        port1.lx = 0x80; port1.ly = 0x80; port1.rx = 0x80; port1.ry = 0x80
        port1.analog = 0
        plat_pad_set(1, &port1)
#if JOJO_HOST
        jojo_host_set_pad(1, port1.buttons, Int32(port1.connected))
#endif
    }

    // MARK: - Controllers

    private func refreshSlots() {
        slots = Array(GCController.controllers().filter { $0.extendedGamepad != nil }.prefix(Int(PLAT_PAD_PORTS)))
    }

    private func readController(_ controller: GCController) -> UInt16 {
        guard let pad = controller.extendedGamepad else { return PadInput.released }
        var word = PadInput.released
        func press(_ button: PS1Button, _ down: Bool) {
            if down { word &= ~button.mask }
        }

        // Positional mapping: A is the bottom face button (cross), B right (circle), X left (square), Y top (triangle).
        press(.cross, pad.buttonA.isPressed)
        press(.circle, pad.buttonB.isPressed)
        press(.square, pad.buttonX.isPressed)
        press(.triangle, pad.buttonY.isPressed)

        press(.l1, pad.leftShoulder.isPressed)
        press(.r1, pad.rightShoulder.isPressed)
        press(.l2, pad.leftTrigger.isPressed)
        press(.r2, pad.rightTrigger.isPressed)
        press(.l3, pad.leftThumbstickButton?.isPressed ?? false)
        press(.r3, pad.rightThumbstickButton?.isPressed ?? false)
        press(.start, pad.buttonMenu.isPressed)
        press(.select, pad.buttonOptions?.isPressed ?? false)

        // D-pad, plus the left stick as a digital d-pad.
        let stickThreshold: Float = 0.5
        let x = pad.leftThumbstick.xAxis.value
        let y = pad.leftThumbstick.yAxis.value
        press(.up, pad.dpad.up.isPressed || y > stickThreshold)
        press(.down, pad.dpad.down.isPressed || y < -stickThreshold)
        press(.left, pad.dpad.left.isPressed || x < -stickThreshold)
        press(.right, pad.dpad.right.isPressed || x > stickThreshold)
        return word
    }

    // MARK: - Keyboard

    private func keyboardWord() -> UInt16 {
        var word = PadInput.released
        for code in keysDown {
            if let button = PadInput.keyMap[code] {
                word &= ~button.mask
            }
        }
        if rightShiftDown {
            word &= ~PS1Button.select.mask
        }
        return word
    }

    private func handle(_ event: NSEvent) {
        switch event.type {
        case .keyDown:
            // Ignore shortcuts such as Cmd-Q or Cmd-O so they do not also press a game button.
            let modifiers = event.modifierFlags.intersection([.command, .control, .option])
            if modifiers.isEmpty {
                keysDown.insert(event.keyCode)
            }
        case .keyUp:
            keysDown.remove(event.keyCode)
        case .flagsChanged:
            if event.keyCode == PadInput.rightShiftKeyCode {
                rightShiftDown = event.modifierFlags.rawValue & PadInput.deviceRightShiftMask != 0
            }
        default:
            break
        }
    }

    private func clearKeyboard() {
        keysDown.removeAll()
        rightShiftDown = false
    }
}
