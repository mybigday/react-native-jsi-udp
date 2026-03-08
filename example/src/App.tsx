import * as React from 'react';

import {
  NativeModules,
  Platform,
  Pressable,
  SafeAreaView,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from 'react-native';

import { isSkipTestError, testSuites } from './tests';
import type { TestCase, TestResult, TestStatus, TestSuite } from './tests';

const STATUS_LABELS: Record<TestStatus, string> = {
  idle: 'Idle',
  running: 'Running',
  passed: 'Passed',
  failed: 'Failed',
  skipped: 'Skipped',
  manual: 'Manual',
};

type FailureResult = TestResult & {
  suiteName: string;
};

type ClipboardModule = {
  setString(content: string): void;
};

const clipboardModule = NativeModules.Clipboard as ClipboardModule | undefined;

function statusColor(status: TestStatus): string {
  switch (status) {
    case 'passed':
      return '#0f766e';
    case 'failed':
      return '#b91c1c';
    case 'running':
      return '#1d4ed8';
    case 'skipped':
      return '#92400e';
    case 'manual':
      return '#6d28d9';
    case 'idle':
    default:
      return '#475569';
  }
}

function toErrorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

function formatDuration(duration: number): string {
  return duration > 0 ? `${duration} ms` : '—';
}

function formatFailureSummary(failures: FailureResult[]): string {
  if (failures.length === 0) {
    return '';
  }

  const heading =
    failures.length === 1 ? '1 failed test' : `${failures.length} failed tests`;
  const sections = failures.map((failure, index) =>
    [
      `${index + 1}. [${failure.suiteName}] ${failure.name}`,
      failure.error ? `Error: ${failure.error}` : undefined,
      failure.details ? `Details: ${failure.details}` : undefined,
      `Duration: ${formatDuration(failure.duration)}`,
    ]
      .filter(Boolean)
      .join('\n')
  );

  return [heading, ...sections].join('\n\n');
}

function createResult(suiteId: string, test: TestCase): TestResult {
  const skipReason = test.skip?.();
  if (skipReason) {
    return {
      suiteId,
      testId: test.id,
      name: test.name,
      status: 'skipped',
      duration: 0,
      details: skipReason,
    };
  }

  if (test.manualInstructions) {
    return {
      suiteId,
      testId: test.id,
      name: test.name,
      status: 'manual',
      duration: 0,
      details: test.manualInstructions,
    };
  }

  return {
    suiteId,
    testId: test.id,
    name: test.name,
    status: 'idle',
    duration: 0,
  };
}

function createInitialResults(): Record<string, TestResult> {
  return Object.fromEntries(
    testSuites.flatMap((suite) =>
      suite.tests.map((test) => [test.id, createResult(suite.id, test)])
    )
  );
}

function summarizeSuite(
  suite: TestSuite,
  results: Record<string, TestResult>
): Record<TestStatus, number> {
  return suite.tests.reduce<Record<TestStatus, number>>(
    (summary, test) => {
      summary[results[test.id].status] += 1;
      return summary;
    },
    {
      idle: 0,
      running: 0,
      passed: 0,
      failed: 0,
      skipped: 0,
      manual: 0,
    }
  );
}

export default function App() {
  const [results, setResults] = React.useState<Record<string, TestResult>>(() =>
    createInitialResults()
  );
  const [isRunning, setIsRunning] = React.useState(false);
  const [activeSuiteId, setActiveSuiteId] = React.useState<string | null>(null);
  const [lastAction, setLastAction] = React.useState('Ready to run tests');

  const summary = React.useMemo(() => {
    return Object.values(results).reduce(
      (counts, result) => {
        counts[result.status] += 1;
        return counts;
      },
      {
        idle: 0,
        running: 0,
        passed: 0,
        failed: 0,
        skipped: 0,
        manual: 0,
      }
    );
  }, [results]);

  const failureResults = React.useMemo<FailureResult[]>(() => {
    return testSuites.flatMap((suite) =>
      suite.tests.flatMap((test) => {
        const result = results[test.id];
        return result.status === 'failed'
          ? [{ ...result, suiteName: suite.name }]
          : [];
      })
    );
  }, [results]);

  const failureSummary = React.useMemo(
    () => formatFailureSummary(failureResults),
    [failureResults]
  );

  const resetResults = React.useCallback((suiteIds?: string[]) => {
    const suiteFilter = suiteIds ? new Set(suiteIds) : undefined;

    setResults((previous) => {
      const next = { ...previous };
      testSuites.forEach((suite) => {
        if (suiteFilter && !suiteFilter.has(suite.id)) {
          return;
        }
        suite.tests.forEach((test) => {
          next[test.id] = createResult(suite.id, test);
        });
      });
      return next;
    });
  }, []);

  const setResult = React.useCallback(
    (suiteId: string, test: TestCase, patch: Partial<TestResult>) => {
      setResults((previous) => ({
        ...previous,
        [test.id]: {
          ...previous[test.id],
          suiteId,
          testId: test.id,
          name: test.name,
          ...patch,
        },
      }));
    },
    []
  );

  const runTest = React.useCallback(
    async (suite: TestSuite, test: TestCase) => {
      const skipReason = test.skip?.();
      if (skipReason) {
        setResult(suite.id, test, {
          status: 'skipped',
          duration: 0,
          details: skipReason,
          error: undefined,
        });
        return;
      }

      if (!test.run) {
        setResult(suite.id, test, {
          status: 'manual',
          duration: 0,
          details:
            test.manualInstructions ?? 'This test must be verified manually.',
          error: undefined,
        });
        return;
      }

      const startedAt = Date.now();
      setResult(suite.id, test, {
        status: 'running',
        duration: 0,
        details: undefined,
        error: undefined,
      });

      try {
        const details = await test.run();
        setResult(suite.id, test, {
          status: 'passed',
          duration: Date.now() - startedAt,
          details: details ?? undefined,
          error: undefined,
        });
      } catch (error) {
        if (isSkipTestError(error)) {
          setResult(suite.id, test, {
            status: 'skipped',
            duration: Date.now() - startedAt,
            details: error.message,
            error: undefined,
          });
          return;
        }

        setResult(suite.id, test, {
          status: 'failed',
          duration: Date.now() - startedAt,
          error: toErrorMessage(error),
          details: undefined,
        });
      }
    },
    [setResult]
  );

  const runSuite = React.useCallback(
    async (suite: TestSuite) => {
      if (isRunning) {
        return;
      }

      setIsRunning(true);
      setActiveSuiteId(suite.id);
      setLastAction(`Running ${suite.name}`);
      resetResults([suite.id]);

      try {
        for (const test of suite.tests) {
          await runTest(suite, test);
        }
        setLastAction(`Finished ${suite.name}`);
      } finally {
        setActiveSuiteId(null);
        setIsRunning(false);
      }
    },
    [isRunning, resetResults, runTest]
  );

  const runAll = React.useCallback(async () => {
    if (isRunning) {
      return;
    }

    setIsRunning(true);
    setActiveSuiteId('all');
    setLastAction('Running all suites');
    resetResults();

    try {
      const runnableSuites = testSuites.filter(
        (suite) => !suite.id.startsWith('latency-server')
      );
      for (const suite of runnableSuites) {
        for (const test of suite.tests) {
          await runTest(suite, test);
        }
      }
      setLastAction('Finished all suites');
    } finally {
      setActiveSuiteId(null);
      setIsRunning(false);
    }
  }, [isRunning, resetResults, runTest]);

  const resetAll = React.useCallback(() => {
    if (isRunning) {
      return;
    }

    setResults(createInitialResults());
    setLastAction('Results reset');
  }, [isRunning]);

  const copyAllErrorLogs = React.useCallback(() => {
    if (!failureSummary) {
      setLastAction('No failed logs to copy');
      return;
    }

    if (!clipboardModule?.setString) {
      setLastAction('Clipboard is unavailable in this runtime');
      return;
    }

    clipboardModule.setString(failureSummary);
    setLastAction(
      `Copied ${failureResults.length} failing ${
        failureResults.length === 1 ? 'log' : 'logs'
      } to the clipboard`
    );
  }, [failureResults.length, failureSummary]);

  return (
    <SafeAreaView style={styles.safeArea}>
      <ScrollView contentContainerStyle={styles.content}>
        <View style={styles.headerCard}>
          <Text style={styles.title}>JSI UDP Example Test Runner</Text>
          <Text style={styles.subtitle}>
            On-device coverage for lifecycle, loopback, address, options,
            multicast, broadcast, errors, stress, and manual suspend/resume
            checks.
          </Text>
          <Text style={styles.meta}>
            Platform: {Platform.OS} · {lastAction}
          </Text>
        </View>

        <View style={styles.summaryRow}>
          {(
            [
              ['passed', summary.passed],
              ['failed', summary.failed],
              ['skipped', summary.skipped],
              ['manual', summary.manual],
              ['idle', summary.idle],
            ] as Array<[TestStatus, number]>
          ).map(([status, count]) => (
            <View
              key={status}
              style={[styles.summaryChip, { borderColor: statusColor(status) }]}
            >
              <Text
                style={[styles.summaryCount, { color: statusColor(status) }]}
              >
                {count}
              </Text>
              <Text style={styles.summaryLabel}>{STATUS_LABELS[status]}</Text>
            </View>
          ))}
        </View>

        <View style={styles.actionsRow}>
          <Pressable
            style={({ pressed }) => [
              styles.primaryButton,
              pressed && styles.buttonPressed,
              isRunning && styles.buttonDisabled,
            ]}
            disabled={isRunning}
            onPress={() => {
              runAll().catch((error) => {
                setLastAction(
                  `Unexpected runner failure: ${toErrorMessage(error)}`
                );
              });
            }}
          >
            <Text style={styles.primaryButtonText}>
              {activeSuiteId === 'all' ? 'Running...' : 'Run All Suites'}
            </Text>
          </Pressable>
          <Pressable
            style={({ pressed }) => [
              styles.secondaryButton,
              pressed && styles.buttonPressed,
              isRunning && styles.buttonDisabled,
            ]}
            disabled={isRunning}
            onPress={resetAll}
          >
            <Text style={styles.secondaryButtonText}>Reset</Text>
          </Pressable>
        </View>

        <Text style={styles.note}>
          Environment-sensitive tests surface explicit skip/manual reasons
          instead of silently passing. Broadcast and multicast are skipped on
          Android emulators because their network stacks are commonly unreliable
          for local loopback validation.
        </Text>

        {failureSummary ? (
          <View style={styles.failureCard}>
            <View style={styles.failureHeader}>
              <View style={styles.failureHeaderText}>
                <Text style={styles.failureTitle}>Failed summary</Text>
                <Text style={styles.failureSubtitle}>
                  Select this summary to copy individual failures, or press the
                  action button to copy every error log at once.
                </Text>
              </View>
              <Pressable
                style={({ pressed }) => [
                  styles.failureCopyButton,
                  pressed && styles.buttonPressed,
                ]}
                onPress={copyAllErrorLogs}
              >
                <Text style={styles.failureCopyButtonText}>
                  Copy all error logs
                </Text>
              </Pressable>
            </View>
            <Text selectable style={styles.failureSummaryText}>
              {failureSummary}
            </Text>
          </View>
        ) : null}

        {testSuites.map((suite) => {
          const suiteSummary = summarizeSuite(suite, results);
          const runningThisSuite = activeSuiteId === suite.id;

          return (
            <View key={suite.id} style={styles.suiteCard}>
              <View style={styles.suiteHeader}>
                <View style={styles.suiteHeaderText}>
                  <Text style={styles.suiteTitle}>{suite.name}</Text>
                  <Text style={styles.suiteDescription}>
                    {suite.description}
                  </Text>
                  <Text style={styles.suiteMeta}>
                    {suiteSummary.passed} passed · {suiteSummary.failed} failed
                    · {suiteSummary.skipped} skipped · {suiteSummary.manual}{' '}
                    manual
                  </Text>
                </View>
                <Pressable
                  style={({ pressed }) => [
                    styles.suiteButton,
                    pressed && styles.buttonPressed,
                    isRunning && styles.buttonDisabled,
                  ]}
                  disabled={isRunning}
                  onPress={() => {
                    runSuite(suite).catch((error) => {
                      setLastAction(
                        `Unexpected ${suite.name} failure: ${toErrorMessage(
                          error
                        )}`
                      );
                    });
                  }}
                >
                  <Text style={styles.suiteButtonText}>
                    {runningThisSuite ? 'Running...' : 'Run Suite'}
                  </Text>
                </Pressable>
              </View>

              {suite.tests.map((test) => {
                const result = results[test.id];
                const tint = statusColor(result.status);

                return (
                  <View key={test.id} style={styles.testRow}>
                    <View style={styles.testHeader}>
                      <Text style={styles.testName}>{test.name}</Text>
                      <View
                        style={[styles.statusBadge, { backgroundColor: tint }]}
                      >
                        <Text style={styles.statusText}>
                          {STATUS_LABELS[result.status]}
                        </Text>
                      </View>
                    </View>
                    <Text style={styles.testDuration}>
                      Duration: {formatDuration(result.duration)}
                    </Text>
                    {result.details ? (
                      <Text selectable style={styles.testDetails}>
                        {result.details}
                      </Text>
                    ) : null}
                    {result.error ? (
                      <Text selectable style={styles.testError}>
                        {result.error}
                      </Text>
                    ) : null}
                  </View>
                );
              })}
            </View>
          );
        })}
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    backgroundColor: '#0f172a',
  },
  content: {
    padding: 16,
    gap: 16,
  },
  headerCard: {
    borderRadius: 16,
    padding: 16,
    backgroundColor: '#111827',
    borderWidth: 1,
    borderColor: '#1f2937',
    gap: 8,
  },
  title: {
    fontSize: 26,
    fontWeight: '700',
    color: '#f8fafc',
  },
  subtitle: {
    fontSize: 15,
    lineHeight: 22,
    color: '#cbd5e1',
  },
  meta: {
    fontSize: 13,
    color: '#94a3b8',
  },
  summaryRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 12,
  },
  summaryChip: {
    minWidth: 92,
    borderRadius: 14,
    borderWidth: 1,
    paddingVertical: 12,
    paddingHorizontal: 14,
    backgroundColor: '#111827',
    alignItems: 'center',
    gap: 4,
  },
  summaryCount: {
    fontSize: 22,
    fontWeight: '700',
  },
  summaryLabel: {
    fontSize: 12,
    color: '#cbd5e1',
    textTransform: 'uppercase',
  },
  actionsRow: {
    flexDirection: 'row',
    gap: 12,
  },
  primaryButton: {
    flex: 1,
    borderRadius: 12,
    paddingVertical: 14,
    paddingHorizontal: 16,
    backgroundColor: '#2563eb',
    alignItems: 'center',
  },
  primaryButtonText: {
    fontSize: 15,
    fontWeight: '700',
    color: '#eff6ff',
  },
  secondaryButton: {
    borderRadius: 12,
    paddingVertical: 14,
    paddingHorizontal: 18,
    backgroundColor: '#1e293b',
    alignItems: 'center',
    justifyContent: 'center',
  },
  secondaryButtonText: {
    fontSize: 15,
    fontWeight: '700',
    color: '#e2e8f0',
  },
  buttonPressed: {
    opacity: 0.85,
  },
  buttonDisabled: {
    opacity: 0.55,
  },
  note: {
    fontSize: 13,
    lineHeight: 20,
    color: '#cbd5e1',
  },
  failureCard: {
    borderRadius: 16,
    padding: 16,
    gap: 12,
    backgroundColor: '#1f1115',
    borderWidth: 1,
    borderColor: '#7f1d1d',
  },
  failureHeader: {
    gap: 12,
  },
  failureHeaderText: {
    gap: 4,
  },
  failureTitle: {
    fontSize: 18,
    fontWeight: '700',
    color: '#fecaca',
  },
  failureSubtitle: {
    fontSize: 13,
    lineHeight: 19,
    color: '#fca5a5',
  },
  failureCopyButton: {
    alignSelf: 'flex-start',
    borderRadius: 10,
    paddingVertical: 10,
    paddingHorizontal: 14,
    backgroundColor: '#7f1d1d',
  },
  failureCopyButtonText: {
    fontSize: 13,
    fontWeight: '700',
    color: '#fee2e2',
  },
  failureSummaryText: {
    fontSize: 13,
    lineHeight: 20,
    color: '#fee2e2',
  },
  suiteCard: {
    borderRadius: 16,
    padding: 16,
    gap: 12,
    backgroundColor: '#111827',
    borderWidth: 1,
    borderColor: '#1f2937',
  },
  suiteHeader: {
    flexDirection: 'row',
    gap: 12,
  },
  suiteHeaderText: {
    flex: 1,
    gap: 4,
  },
  suiteTitle: {
    fontSize: 20,
    fontWeight: '700',
    color: '#f8fafc',
  },
  suiteDescription: {
    fontSize: 14,
    lineHeight: 20,
    color: '#cbd5e1',
  },
  suiteMeta: {
    fontSize: 12,
    color: '#94a3b8',
  },
  suiteButton: {
    alignSelf: 'flex-start',
    borderRadius: 10,
    paddingVertical: 10,
    paddingHorizontal: 14,
    backgroundColor: '#334155',
  },
  suiteButtonText: {
    fontSize: 13,
    fontWeight: '700',
    color: '#f8fafc',
  },
  testRow: {
    borderRadius: 12,
    padding: 12,
    gap: 6,
    backgroundColor: '#0f172a',
    borderWidth: 1,
    borderColor: '#1e293b',
  },
  testHeader: {
    flexDirection: 'row',
    gap: 10,
    alignItems: 'center',
  },
  testName: {
    flex: 1,
    fontSize: 15,
    fontWeight: '600',
    color: '#f8fafc',
  },
  statusBadge: {
    borderRadius: 999,
    paddingVertical: 4,
    paddingHorizontal: 10,
  },
  statusText: {
    fontSize: 11,
    fontWeight: '700',
    color: '#f8fafc',
    textTransform: 'uppercase',
  },
  testDuration: {
    fontSize: 12,
    color: '#94a3b8',
  },
  testDetails: {
    fontSize: 13,
    lineHeight: 18,
    color: '#cbd5e1',
  },
  testError: {
    fontSize: 13,
    lineHeight: 18,
    color: '#fca5a5',
  },
});
