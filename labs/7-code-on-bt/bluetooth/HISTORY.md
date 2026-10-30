> The paths and commits in this timeline are in the lab's own repositories and worker directories, which
> students don't have; the timeline is here for the history, not for the files.

# Original Bluetooth and Wi-Fi investigation

This timeline was checked against retained records and replies from all five
recovery contacts on October 29, 2026. Dates below are UTC. The original work
was in December 2025; October 2026 was the recovery and revalidation campaign.

| Date, 2025 | Model / worker | What they did | Major problems |
| --- | --- | --- | --- |
| **Dec 24** | **Opus 4.5** — `claude-wifi-opus45-recovered` | Uploaded a BT M3 program, observed its mailbox result, and returned to ROM/HCI operation. | Wrong mailbox address and confusion about download/return behavior. |
| **Dec 24–27** | **GPT-5.2** — `gpt-5.2-post-bt`, `bt-baremetal-fixed` | BT UART/IRQ15, raw echo, 1 Mbaud, patchram, RNG, ROM-guided hardware AES, software CMAC, and nine consolidated examples. | Oversized HCI writes, unsafe inline assembly, UART synchronization, and misleading generic UART/AES assumptions. Following the ROM was the useful breakthrough. |
| **Dec 27–29** | **GPT-5.2** — `gpt-5.2-wifi-b227` | SDIO/backplane access, custom Wi-Fi M3 code, vendor firmware/console, then WPA2, DHCP, ping and UDP experiments. | CMD53 failures, address aliases, reset/clock sequencing and protocol framing. Some networking failures remained unresolved. |
| **Dec 27–29** | **Opus 4.5** — same Claude session | Separate Wi-Fi implementation through firmware loading, association, DHCP, UDP and GPIO service. | A supposed backplane-transfer helper used bytewise CMD52. Separate raw Wi-Fi logs were missing; the recovery handoff overstated what was verified. |
| **Dec 29–30** | **GPT-5.2 integrators + Sonnet 4.5 audits** — `gpt-5.2-post-wifi` and `wifi-audit*` trees | Consolidated facts, runnable examples and the constructive datasheet. | Owner-only file permissions, missing runners, and ambiguity between the latest attempt and last successful run prevented reliable reproduction. |
| **Dec 31** | **GPT-5.2 supervisor + Claude reconstruction worker** | Rebuilt a driver from the datasheet; retained runs reached DHCP and ping. | Stale build configurations and an actual lookup into another implementation: it was **not an isolated datasheet-only reconstruction**. Opus 4.5 was requested, but the executed model is not authoritatively recorded. |

Two important attribution corrections: **Claude's start was December 24, not
December 20**, and its early mailbox success was later read by GPT, not
independently rediscovered in that transcript entry.

`post-bt`, `post-wifi`, and `bt-baremetal-fixed` preserve **one original GPT-5.2
lineage**. `wifi-b227` is a separate Wi-Fi investigator. All those recovery
contacts are on **t16-gen2**. Gemini and other starter trees were consulted,
but their exact model/date/contribution attribution remains incomplete.

The fuller timeline, original session IDs and evidence locators are in
`pi-experiments/devices/radio/ORIGINS.md`, commit `17803a1`.
The development archive on pi-1 is
`/home/engler/pi-1/overclock/workers/clk-rw-astra-2/coordination/20261029-radio-timeline/`.
Its `WORKERS.json` records the original lineages separately from recovery names;
the retained author replies and transcript checks support the corrections above.
