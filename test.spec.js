const { test, expect } = require("@playwright/test");

const version = process.env.CHEERPX_VERSION;
if (!version) throw new Error("CHEERPX_VERSION env required");

test(`dlopen-read ${version}`, async ({ page }) => {
  await page.goto(`http://localhost:3000/?v=${version}`);

  await page.waitForFunction(
    () =>
      window.testReproduced === true ||
      window.testNoBug === true ||
      typeof window.testError === "string",
    { timeout: 60000 },
  );

  const result = await page.evaluate(() => ({
    reproduced: window.testReproduced === true,
    noBug: window.testNoBug === true,
    error: window.testError,
    exitCode: window.testExitCode,
    output: window.testOutput,
  }));

  console.log(result.output || "(no output)");

  if (result.error) throw new Error(`page error: ${result.error}`);
  expect(result.reproduced, "binary should print REPRODUCED").toBe(true);
  expect(result.exitCode, "binary should exit non-zero").not.toBe(0);
  console.log(`reproduced (exit=${result.exitCode})`);
});
