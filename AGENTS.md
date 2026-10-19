# 开发流程

- 开发前先阅读并遵循父项目 YGOPro 的 `AGENTS.md`。当前子模块布局中的路径是 `../AGENTS.md`。
- 修改 C++ 代码或依赖后，必须完成 WASM、SO 和父项目的编译验证；有任何失败都应修复后重新验证。
  - 在本目录运行 `bash -e ./build-wasm.sh`，确认 CJS 和 ESM 两个目标均成功生成新的 wasm 与加载文件。
  - 在本目录运行 `bash -e ./build-so.sh`，确认生成新的 `libocgcore.so`。
  - 在父项目目录运行 `JOBS=4 ./ygopro-build.sh`。
  - 前两个脚本用 CPU affinity 限制为四核并行；缺少本机工具时使用 Docker，不在本机安装。
- 编译完成后必须通过真实消费项目运行本次改动的回归测试，不能仅以编译成功或模块加载成功作为验收。
  - 先阅读 `../../ygopro-scripting-workflow/AGENTS.md` 并遵循其中的测试要求。
  - 新建 `../../ygopro-scripting-workflow/workspace/<YYYYMMDD>-<task>/`，使用实际执行日期；将本次生成的 CJS/ESM wasm 与对应加载文件复制到其中，核对源文件与副本的 SHA-256。
  - 测试放在该项目对应的 `tests/workspace/<YYYYMMDD>-<task>/` 下；测试辅助 fixture 遵循该项目的归档约定。
  - 在 `createTest` 中通过 `ocgcoreOptions: { wasmBinary: readFileSync(wasmPath) }` 显式加载日期工作区内的新 wasm，分别验证 CJS/ESM 产物，避免误用包内或全局默认 wasm。
  - 若消费包内的 JS 加载器与本次 wasm 不兼容，在日期工作区中配置隔离的 Jest `moduleNameMapper`，指向本次生成的对应加载文件；不要覆盖公共包或全局 wasm。
  - 测试覆盖实际行为和相关边界，遵循 unit、e2e、MSG 断言和 Lua coverage 要求；编辑测试后运行 `npm run lint`，新增或修改卡片脚本时完成红字检查。
- 全部编译和回归通过后再提交或推送；记录实际命令、测试结果、产物位置与提交信息。
