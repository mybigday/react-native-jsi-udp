---
name: maestro
description: Run Maestro UI tests on the example app in iOS/Android simulators. Use when testing UI flows, verifying test suite results, automating the example app, running maestro commands, creating test flows, or validating changes via the on-device test runner. Triggers on requests involving Maestro, UI testing, running example tests, or verifying fixes on-device.
---

# Maestro UI Testing

## Setup

```bash
# Install (if needed)
curl -fsSL "https://get.maestro.mobile.dev" | bash

# Verify
maestro --version
```

## Running Tests

```bash
# Target iOS simulator (get UDID from `xcrun simctl list devices available`)
maestro --device <SIMULATOR_UDID> test example/maestro/flow.yaml

# Target Android (default if connected)
maestro test example/maestro/flow.yaml

# Run all flows
maestro --device <UDID> test example/maestro/

# Generate HTML report
maestro --device <UDID> test --format html --output results.html example/maestro/
```

## Building & Launching the Example App

Before running Maestro, the example app must be built and installed:

```bash
# Install deps & pods
cd /path/to/react-native-jsi-udp && yarn install
cd example/ios && pod install

# Build Release for simulator
xcodebuild -workspace JsiUdpExample.xcworkspace -scheme JsiUdpExample \
  -configuration Release -sdk iphonesimulator \
  -destination 'platform=iOS Simulator,name=<DEVICE_NAME>' build

# Install & launch
xcrun simctl install <UDID> "<BUILT_PRODUCTS_DIR>/JsiUdpExample.app"
xcrun simctl launch <UDID> org.reactjs.native.example.JsiUdpExample
```

For Debug builds, also start Metro: `cd example && yarn start`

## Writing Flows

See [references/maestro.md](references/maestro.md) for the full Maestro command reference.

App ID: `org.reactjs.native.example.JsiUdpExample`

### Example App Structure

The example app is a test runner with suites: lifecycle, send-receive, address, options, multicast, broadcast, errors, stress, suspend-resume, and latency-server.

Key UI elements:
- **"Run All Suites"** button — runs all test suites (except latency-server)
- **"Run Suite"** button — per-suite, multiple on screen (use `index:` to target)
- **"Reset"** button — resets all results
- Suite status text: `"N passed · N failed · N skipped · N manual"` (single text node, use regex if matching substrings)
- Test names are visible text elements
- Status badges: `"PASSED"`, `"FAILED"`, `"SKIPPED"`, `"MANUAL"`, `"RUNNING"`

### Flow Tips

- Use `launchApp: { clearState: true }` for clean runs
- Tests run fast — use `extendedWaitUntil` for "Run Suite" button reappearing as completion signal
- Compound text nodes (e.g., "4 passed · 0 failed") can't be matched by substring; use `scrollUntilVisible` + `assertVisible` on individual elements instead
- Always specify `--device <UDID>` on machines with both iOS simulator and Android connected
- Use `scrollUntilVisible` to find tests that are below the fold

### Minimal Flow Template

```yaml
appId: org.reactjs.native.example.JsiUdpExample
name: Run and verify tests
---
- launchApp:
    clearState: true
- extendedWaitUntil:
    visible: "Run All Suites"
    timeout: 10000
- tapOn: "Run All Suites"
- extendedWaitUntil:
    visible: "Run All Suites"
    timeout: 120000
- takeScreenshot: "results"
```
