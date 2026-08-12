@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo.
echo ============================================================
echo   EvolveMusic Cloud - Cloudflare setup
echo ============================================================
echo.

if not exist wrangler.jsonc (
  copy /y wrangler.example.jsonc wrangler.jsonc >nul
  echo Created wrangler.jsonc from the public example.
)

call npm install
if errorlevel 1 exit /b 1

call npx wrangler login
if errorlevel 1 exit /b 1

echo.
echo [1] Creating D1 database...
call npx wrangler d1 create evolvemusic-db

echo.
echo IMPORTANT:
echo Copy the D1 database_id shown above into:
echo   wrangler.jsonc
echo replacing:
echo   REPLACE_WITH_D1_DATABASE_ID
echo.
echo Then run this file again with argument deploy:
echo   setup_windows.bat deploy
echo.
if /I not "%~1"=="deploy" exit /b 0

findstr /C:"00000000-0000-0000-0000-000000000000" wrangler.jsonc >nul 2>nul
if not errorlevel 1 (
  echo ERROR: wrangler.jsonc still contains the example D1 database ID.
  exit /b 1
)

echo.
echo [2] Creating R2 bucket if needed...
call npx wrangler r2 bucket create evolvemusic-audio

echo.
echo [3] Applying D1 schema...
call npx wrangler d1 execute evolvemusic-db --remote --file=./schema.sql
if errorlevel 1 exit /b 1

echo.
echo [4] Set these secrets when Wrangler prompts:
echo.
echo   npx wrangler secret put SESSION_MASTER_KEY
echo   npx wrangler secret put STREAM_SIGNING_KEY
echo   npx wrangler secret put ADMIN_KEY
echo.
echo After the secrets are set, deploy with:
echo   npx wrangler deploy
echo.
