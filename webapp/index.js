import express from "express";
import { spawn } from "child_process";
import fs from "fs/promises";
import path from "path";
import { fileURLToPath } from "url";
import crypto from "crypto";
import cors from "cors";
import multer from "multer";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const upload = multer({ dest: path.join(__dirname, "tmp") });

const app = express();
const PORT = 3000;

app.use(cors());

app.use(express.json({ limit: "10mb" }));
app.use(express.static(path.join(__dirname, "public")));

/**
 * GET /api/health
 * Returns: { "ok": true }
 */
app.get("/api/health", (req, res) => {
  res.status(200).json({ ok: true });
});

/**
 * POST /api/analyze
 */
app.post("/api/analyze", async (req, res) => {
  try {
    const inputJson = req.body;

    // Basic validation
    if (!inputJson || !inputJson.raw_tx) {
      return res.status(400).json({
        ok: false,
        error: "Invalid input JSON",
      });
    }

    // Create temp file
    const tmpDir = path.join(__dirname, "tmp");
    await fs.mkdir(tmpDir, { recursive: true });

    const fileName = `tx-${crypto.randomUUID()}.json`;
    const filePath = path.join(tmpDir, fileName);

    await fs.writeFile(filePath, JSON.stringify(inputJson, null, 2));

    // Spawn C++ analyzer
    const analyzerPath = path.join(__dirname, "analyzer");

    const child = spawn(analyzerPath, [filePath]);

    let stdoutData = "";
    let stderrData = "";

    child.stdout.on("data", (data) => {
      stdoutData += data.toString();
    });

    child.stderr.on("data", (data) => {
      stderrData += data.toString();
    });

    child.on("close", async (code) => {
      // Delete temp file
      await fs.unlink(filePath).catch(() => {});

      if (code !== 0) {
        return res.status(500).json({
          ok: false,
          error: "Analyzer failed",
          details: stderrData,
        });
      }

      try {
        const outputJson = JSON.parse(stdoutData);
        return res.status(200).json(outputJson);
      } catch (err) {
        return res.status(500).json({
          ok: false,
          error: "Invalid JSON from analyzer",
          raw_output: stdoutData,
        });
      }
    });
  } catch (err) {
    return res.status(500).json({
      ok: false,
      error: err.message,
    });
  }
});

app.post(
  "/api/analyze-block",
  upload.fields([
    { name: "blk", maxCount: 1 },
    { name: "rev", maxCount: 1 },
    { name: "xor", maxCount: 1 },
  ]),
  async (req, res) => {
    const blkFile = req.files?.blk?.[0];
    const revFile = req.files?.rev?.[0];
    const xorFile = req.files?.xor?.[0];

    if (!blkFile || !revFile || !xorFile) {
      return res.status(400).json({
        ok: false,
        error: {
          code: "MISSING_FILES",
          message: "blk, rev, and xor files are all required",
        },
      });
    }

    const outDir = path.join(__dirname, "out"); // ← moved here, top of handler

    const analyzerPath = path.join(__dirname, "analyzer");
    await fs.rm(outDir, { recursive: true, force: true });
    await fs.mkdir(outDir, { recursive: true });

    const child = spawn(analyzerPath, [
      "--block",
      blkFile.path,
      revFile.path,
      xorFile.path,
    ]);

    let stderrData = "";
    child.stderr.on("data", (d) => (stderrData += d.toString()));

    child.on("close", async (code) => {
      await Promise.all([
        fs.unlink(blkFile.path).catch(() => {}),
        fs.unlink(revFile.path).catch(() => {}),
        fs.unlink(xorFile.path).catch(() => {}),
      ]);

      if (code !== 0) {
        return res.status(500).json({
          ok: false,
          error: {
            code: "BLOCK_PARSE_FAILED",
            message: stderrData || "Analyzer exited with error",
          },
        });
      }

      try {
        const files = await fs.readdir(outDir);
        const jsonFiles = files.filter((f) => f.endsWith(".json"));
        const results = await Promise.all(
          jsonFiles.map(async (f) => {
            const content = await fs.readFile(path.join(outDir, f), "utf8");
            return JSON.parse(content);
          }),
        );

        if (results.length === 1) return res.status(200).json(results[0]);
        return res.status(200).json({ ok: true, blocks: results });
      } catch (err) {
        return res.status(500).json({
          ok: false,
          error: { code: "READ_OUTPUT_FAILED", message: err.message },
        });
      }
    });
  },
);

app.listen(PORT, () => {

});
