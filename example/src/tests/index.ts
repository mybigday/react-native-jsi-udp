export {
  isSkipTestError,
  skipTest,
  type TestCase,
  type TestResult,
  type TestStatus,
  type TestSuite,
} from './helper';

import { addressSuite } from './address';
import { broadcastSuite } from './broadcast';
import { errorsSuite } from './errors';
import { latencyServerSuite } from './latency-server';
import { latencyServerRNUDPSuite } from './latency-server-bridge';
import { lifecycleSuite } from './lifecycle';
import { multicastSuite } from './multicast';
import { optionsSuite } from './options';
import { sendReceiveSuite } from './send-receive';
import { stressSuite } from './stress';
import { suspendResumeSuite } from './suspend-resume';

export const testSuites = [
  lifecycleSuite,
  sendReceiveSuite,
  addressSuite,
  optionsSuite,
  multicastSuite,
  broadcastSuite,
  errorsSuite,
  stressSuite,
  suspendResumeSuite,
  latencyServerSuite,
  latencyServerRNUDPSuite,
];
