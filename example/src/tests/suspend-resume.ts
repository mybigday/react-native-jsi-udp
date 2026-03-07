import { Platform } from 'react-native';
import { type TestSuite } from './helper';

export const suspendResumeSuite: TestSuite = {
  id: 'suspend-resume',
  name: 'Suspend / resume',
  description:
    'Documents the manual iOS validation for backgrounding and resuming bound sockets.',
  tests: [
    {
      id: 'suspend-resume-ios-manual',
      name: 'manual iOS socket recovery after background/foreground',
      skip: () =>
        Platform.OS === 'ios'
          ? undefined
          : 'Manual suspend/resume validation only applies to iOS.',
      manualInstructions:
        'Run the Send / Receive suite first, background the app for a few seconds, return to the foreground, and rerun the loopback tests to confirm sockets still receive packets.',
    },
  ],
};
