# Maestro - Mobile UI Testing Framework

Reference: https://github.com/mobile-dev-inc/Maestro

Maestro is a UI testing framework for mobile apps with built-in tolerance for flakiness and delays. It uses declarative YAML syntax for defining test flows.

## Installation

```bash
# macOS / Linux
curl -fsSL "https://get.maestro.mobile.dev" | bash

# Verify
maestro --version
```

## CLI Commands

```bash
# Run a single flow
maestro test flow.yaml

# Run all flows in a folder
maestro test myFlows/

# Run with environment variables
maestro test -e APP_ID=com.example.app -e USERNAME=test flow.yaml

# Generate JUnit report
maestro test --format junit myFlows/

# Generate HTML report
maestro test --format html --output results.html myFlows/

# Launch Maestro Studio (visual flow builder in browser)
maestro studio

# Run in cloud CI
maestro cloud --api-key <API_KEY> --project-id <PROJECT_ID> app.zip flow.yaml
```

## Flow File Structure

```yaml
# flow.yaml
appId: com.example.app
name: My Test Flow
tags:
  - smoke
  - login
env:
  USERNAME: ${USERNAME || "default@example.com"}
  PASSWORD: ${PASSWORD || "password123"}
onFlowStart:
  - runScript: setup.js
onFlowComplete:
  - takeScreenshot: final-state
---
# Commands go here after the --- separator
- launchApp
- tapOn: "Sign In"
```

## Core Commands

### App Lifecycle

```yaml
- launchApp                    # Launch app
- launchApp:
    clearState: true           # Launch with cleared state
    clearKeychain: true        # Also clear keychain (iOS)
- stopApp                      # Stop the app
- clearState                   # Clear app state without relaunch
- clearKeychain                # Clear iOS keychain
```

### Tap & Press

```yaml
- tapOn: "Button Text"         # Tap by visible text
- tapOn:
    id: "button_id"            # Tap by testID / accessibilityIdentifier
- tapOn:
    point: 50%,50%             # Tap by screen coordinates (relative)
- tapOn:
    text: "Button"
    index: 0                   # Tap nth match (0-indexed)
    retryTapIfNoChange: false  # Don't retry if state unchanged
- tapOn:
    text: "Button"
    repeat: 3                  # Repeat tap 3 times
    delay: 500                 # Delay between taps (ms)
- longPressOn: "Element"       # Long press by text
- longPressOn:
    id: "view_id"              # Long press by ID
- doubleTapOn: "Element"       # Double tap
```

### Text Input

```yaml
- tapOn:
    id: "input_field"          # Focus the field first
- inputText: "Hello World"     # Type text into focused field
- eraseText                    # Erase all text in focused field
- eraseText:
    charactersToErase: 5       # Erase specific number of characters
- hideKeyboard                 # Dismiss keyboard
- copyTextFrom:
    id: "text_view"            # Copy text from element
- pasteText                    # Paste clipboard content
```

### Assertions

```yaml
- assertVisible: "Welcome"                # Assert text is visible
- assertVisible:
    id: "element_id"                       # Assert element by ID
    enabled: true                          # Assert enabled state
- assertNotVisible: "Error"               # Assert not visible
- assertVisible:
    text: "Count: .*"
    isRegex: true                          # Regex matching
```

### Scrolling & Swiping

```yaml
- scroll                       # Scroll down
- scrollUntilVisible:
    element:
      id: "target_element"
    direction: DOWN            # DOWN | UP | LEFT | RIGHT
    timeout: 20000             # Timeout in ms (default: 20000)
    speed: 40                  # 0-100 (default: 40)
- swipe:
    direction: LEFT            # LEFT | RIGHT | UP | DOWN
    duration: 400              # Duration in ms
- swipe:
    start: 90%,50%             # From point
    end: 10%,50%               # To point
```

### Navigation

```yaml
- back                         # Press back button (Android) / swipe back (iOS)
- pressKey: Home               # Press hardware key
- openLink: "myapp://deep-link"  # Open deep link
```

### Wait & Timing

```yaml
- extendedWaitUntil:
    visible: "Element"
    timeout: 10000             # Wait up to 10s for element
- waitForAnimationToEnd        # Wait for animations to finish
```

### Screenshots & Media

```yaml
- takeScreenshot: "my-screenshot"    # Save screenshot with name
- startRecording: "my-recording"     # Start video recording
- stopRecording                      # Stop video recording
- addMedia:
    - "path/to/image.png"           # Add media to device
```

### JavaScript Evaluation

```yaml
# Inline JavaScript in commands
- inputText: ${1 + 1}                       # Inputs "2"
- inputText: ${'Hello ' + MY_NAME}          # Variable interpolation
- tapOn: ${MY_NAME}                          # Dynamic selector

# evalScript for variable management
- evalScript: ${output.counter = 0}
- evalScript: ${output.counter = output.counter + 1}

# runScript for complex logic
- runScript: myScript.js
```

### Control Flow

```yaml
# Conditional execution
- runFlow:
    when:
      visible: "Login"         # Only run if "Login" visible
    commands:
      - tapOn: "Login"

- runFlow:
    when:
      notVisible: "Welcome"    # Only run if "Welcome" not visible
    commands:
      - launchApp

# Repeat
- repeat:
    times: 3                   # Repeat N times
    commands:
      - tapOn: "Next"

# Repeat while condition
- repeat:
    while:
      visible: "Loading"       # Repeat while element visible
    commands:
      - scroll

# Repeat with variable condition
- evalScript: ${output.done = 0}
- repeat:
    while:
      true: ${output.done == 0}
    commands:
      - swipe:
          start: 50%,90%
          end: 50%,75%
      - runFlow:
          when:
            visible:
              id: "target"
          commands:
            - evalScript: ${output.done = 1}

# Sub-flows
- runFlow: login.yaml          # Run another flow file
- runFlow:
    file: login.yaml
    env:
      USERNAME: "admin"        # Pass variables to sub-flow
```

### Selectors

Elements can be selected by multiple properties:

```yaml
# By text
- tapOn: "Button Text"
- tapOn:
    text: "Button Text"

# By ID (testID / accessibilityIdentifier)
- tapOn:
    id: "my_button"

# By point (relative %)
- tapOn:
    point: 50%,50%

# Combined selectors
- tapOn:
    text: "Submit"
    index: 0                   # First match
    enabled: true              # Must be enabled

# Optional selector (don't fail if not found)
- tapOn:
    id: "dismiss_button"
    optional: true
```

## React Native Notes

- Use `testID` prop on RN components for reliable element selection
- `testID` maps to `accessibilityIdentifier` on iOS, `resource-id` on Android
- Prefer `id` selectors over `text` for stability across languages/themes
- Tap `TextInput` first before using `inputText` (RN needs explicit focus)
- Use `maestro studio` to inspect element hierarchy and discover selectors
