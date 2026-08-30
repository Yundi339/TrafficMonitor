# Hardware monitor runtime dependencies

Source: [LibreHardwareMonitor v0.9.6](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases/tag/v0.9.6), `LibreHardwareMonitor.zip`.

The files below are the transitive .NET Framework runtime dependency closure of `LibreHardwareMonitorLib.dll`. Full builds must ship all of them next to `TrafficMonitor.exe`.

| File | Assembly version | SHA-256 |
| --- | --- | --- |
| `LibreHardwareMonitorLib.dll` | 0.9.6.0 | `6EBC194316536BA61AF5BE24508AD9FCBB2ECC685E716C12E787C79530F66BF0` |
| `BlackSharp.Core.dll` | 1.0.7.0 | `CAFB93AFCC8D8A367E21F619673D05C06887D8964867FED1371F02DED1CD3E23` |
| `DiskInfoToolkit.dll` | 1.1.2.0 | `1ACBF51B3C10C51C986CF43021680D34A2E38D9A5BA652BCFA9A1B5F7FC09800` |
| `HidSharp.dll` | 2.6.4.0 | `D86690EFDE30EA9179F669320F39148853793B743A98B531AFEAF30598E22F54` |
| `RAMSPDToolkit-NDD.dll` | 1.4.2.0 | `B6882354C7C8EC186617E421507743DBFAE09C5C1FC24CEF76A1D0C0C26651DE` |
| `System.Buffers.dll` | 4.0.5.0 | `2D78D770C9CB997199154AE8C018B9F1D1EFBC86729F7264DDE6DBAD2A12CAC3` |
| `System.Memory.dll` | 4.0.5.0 | `D5E8E4866F9CFA66F7765660F84B210198893E55335487AFE5EBDA342C0E913D` |
| `System.Numerics.Vectors.dll` | 4.1.6.0 | `20C2FA81B8C70D651099D762954F285FD4F942E63B2D7217C145DAB8D4B2F4C9` |
| `System.Runtime.CompilerServices.Unsafe.dll` | 6.0.3.0 | `08CBD7278B66F1E68425A82D4B97181A4130D93E3DD91831407ABA7212CCDACF` |

Do not update only `LibreHardwareMonitorLib.dll`: missing transitive assemblies can prevent the Full edition from starting or initializing hardware monitoring.
