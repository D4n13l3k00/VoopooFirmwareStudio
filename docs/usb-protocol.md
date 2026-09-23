# Built-in VOOPOO USB flasher

This is an independent implementation of the installed native Qt updater's
ZWDF transport. It does not launch, load or redistribute `voopoo.exe` or its DLLs.
It is not the older .NET updater's `A1 ... 30` protocol.

## Reference and evidence

Reference: `C:\Program Files (x86)\voopoo\voopoo.exe`, 614400 bytes.
SHA-256: `1286070655dcd5997db16ea04420a4a2b5462c871c8e6694dc89912de08377be`.
Addresses below are virtual addresses in that specific x86 executable.

| Function | Recovered behavior |
|---|---|
| `0x4186D0` | CRC16, initial zero, polynomial `0x1021` |
| `0x419310` | Split into HID reports of 64 bytes, report ID `02`, 63 data bytes |
| `0x4195B0` | Receive packet and verify CRC |
| `0x419890` | Device error `FA`, reason `01` STOP or `0A` LOWPOWER |
| `0x419C10`, `0x41AFA0` | Discovery `44`, fallback `36`; hardware ID is the response address |
| `0x41A590` | Enter bootloader, command `37`, 16 bytes `voopo-test-0000\0` |
| `0x41A670` | Prepare `09` below `0x12000` image bytes; otherwise `08` |
| `0x41A730` | Write `04`, big-endian byte offset, up to 512 image bytes with `+0x33` |
| `0x41AA20` | Finalize `20`, big-endian image length and additive checksum |
| `0x41AC30` | Prepare, block 0 prelude, prepare again, all blocks from zero, finalize |

`tests/official_vectors.json` contains packets captured by executing only the
original packet-builder instructions in an isolated x86 interpreter, intercepting
the call to `0x419AA0` before transport. The test input is 524 bytes, byte `i` is
`i % 256`, hardware code `0x3103`. This exercise did not load the updater as a
Windows process or access USB. The same vectors are compiled into
`tests/official_vectors.h` for offline comparisons.

The firmware's ACK builder (`0x0800FEB0` in the analyzed 3103 application)
uses `request_command | 0x80`; errors use `FA` with a reason byte. Strict reply
correlation is required by this implementation. Compatibility with the actual
device's bootloader responses still requires a hardware test.

## Wire format

```text
68  address_BE16  command  payload_length_BE16  payload  CRC16_LE  16
```

CRC covers the address through the end of the payload, excluding `68` and the
trailing CRC/`16`. The last HID report is padded with zeros. Only report ID `02`
and exactly 64-byte reports are accepted. Received frame length is bounded to
4096 bytes; framing, length, CRC, response command and address must all match.

The updater targets one matching VOOPOO device (`0483:5750`), with the expected
input/output report lengths. Multiple matching HID interfaces are refused.
Discovery addresses `AAAA`; replies must contain a concrete hardware ID. Before
any update command, that ID must match the selected package record exactly.
The UI targets **`0x3103`**, not decimal `3103`.

After command `37`, a fresh `VOOPOO-BOOT` identity with the same hardware ID is
required. Serial numbers are compared when both modes expose one. An empty
boot-entry response can represent a reset/disconnect; an explicit rejection or
corrupt response stops the update. Matching ID alone cannot distinguish two
physical boards of the same model without stable serial numbers.

Image bytes come from the parsed record, excluding its trailing two-byte hardware
ID and package metadata. Final checksum is the 32-bit sum of these raw bytes,
not the sum after `+0x33`. Full blocks and the final short block are sent; an
exact multiple of 512 does not produce an extra empty block.

## Deliberate differences and limits

- Every preparation, prelude, data and final response must be acknowledged.
  STOP/LOWPOWER, corrupt or unrelated responses, and timeouts stop immediately.
- There are no blind retries after an ambiguous write/timeout. The reference
  updater retries some commands; doing so could accept a delayed ACK as a reply
  to another block because this protocol's ACK does not include a block index.
- Storage preparation waits up to 3/6 seconds (second small prepare: 2 seconds),
  block ACKs 500 ms and final ACK 1000 ms. Read timeouts apply to the whole reply.
- Monitoring and flashing serialize USB access. Timed-out overlapped I/O is
  cancelled and drained before buffers/events are released.
- A positive final ACK means the device accepted the final length/checksum
  command. It is not a Flash read-back or proof that the firmware booted.
- There is no SWD erase, unlock, MCU option-byte editing, or direct Flash address
  write path here. USB storage placement is controlled by the device bootloader.
- Unverified battery, puff-count and resistance offsets are not shown as telemetry.

## Offline verification

Build either Flasher configuration, then run `tests/Verify.ps1`. `--smoke-test`
does not start USB monitoring. Tests compare original packet vectors and the
complete sequence, inject failures at all six stages of the synthetic update,
and exercise cancellation, target mismatch, duplicate records, frame corruption,
512-byte boundaries and the large-image command threshold.

These checks validate packet generation and error handling without writing a
device. They do not establish the cause of the previously reported black screen
or demonstrate recovery of a physical Drag S.
