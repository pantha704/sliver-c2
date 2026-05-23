# UnDefend - Windows Defender DOS Tool
Author: Nightmare-Eclipse (@Nightmare-Eclipse, 4.7k followers, "Microsoft's nightmare")
Stars: 541 | Language: C++

## Description
Blocks Windows Defender from receiving signature updates. No admin privileges required.

## How it works
1. Finds Defender's signature update directory via registry
2. Monitors for new files (ReadDirectoryChangesW)
3. When a new signature arrives, locks it with exclusive file lock (LockFileEx)
4. Also locks backup files (mpavbase.lkg, mpavbase.vdm)
5. WDKillerThread: if Defender service restarts, re-locks all files

## Modes
- **Passive (default)**: Blocks ALL signature updates. Defender can't detect NEW threats.
- **Aggressive**: Waits for major platform update (MsMpEng.exe update), then disables Defender completely.

## Compile
```bash
zig c++ -target x86_64-windows-gnu -mconsole -o UnDefend.exe UnDefend.cpp \
  -lntdll -lkernel32 -lole32 -ladvapi32 -luser32
```

## Note
Author claims they also found a way to fake the EDR web console (show Defender as running/updating when it's not) but never published it.
