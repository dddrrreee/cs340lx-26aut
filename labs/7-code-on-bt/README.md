## Running arbitrary code on the pi's Bluetooth and WiFi chip

Today is a fun lab: run arbitrary-ish code on the pi's Bluetooth and
WiFi chips.  These are actually two M3 ARM processors and while they
lack some things such as virtual memory, they have a full CPU, exceptions
and a bunch of other gizmos.  

Context: Bluetooth and WiFi are complicated.  Rather than burn
this complexity into hardware, which inevitably burns bugs as well,
the common approach is to put a real CPU on the chip and have it run
programs to implement the device.  People call this code "device firmware"
but it's usually just regular code with a lot of arbitrary knowledge.

We're going to run our own code instead of the vendors.  You can do some
very interesting tricks by having a separate co-processor, especially
one that has access to the interesting WiFi and Bluetooth hardware.
A follow on lab with have a bunch of "choose your own adventure" cards
to pick from to do some of these tricks.

This specific project has a lot of sentimental value for me.  It's part
of the reason that I got so AGI pilled so hard last fall.   (Ballpark:
12-24(unfortunately) hours most days since then with 4-40 sessions going.)
The hardware is not well documented, you have to do lots of archeology to
figure it out but! --- you do have a sort-of oracle in that you can run
experiments on.  If you do dispositive experiments, you can hill-climb.
And if you hill-climb there are interesting mountains.

I spent a bunch of time over Thanksgiving and Christmas reverse
engineering the chips and my first ASI moment was when Claude finished
the WiFi driver, sent packets to a pi running it, and used laptop webcam
to check that it worked.  

Some screenshots from that session when opus 4.5 finally succeeded:

Success:
<div align="center">
<img src="images/claude-blink0.png" width="400" />
</div>

Celebration:
<div align="center">
<img src="images/claude-blink.png" width="400" />
</div>

Wow:
<div align="center">
<img src="images/claude-matrix.png" width="400" />
</div>

Last week I resurrected those sessions, debriefed them, and spent
a bunch of days redoing everything.  Much time was spent doing 
aggressive validation on hardware with negative tests (where you
break something and make sure you see an expected failure).  The ease
that you can do violent validation has been one of the shiny objects
I've been chasing over the past year.

The result is a statement I never thought I would type: I then had
Opus 5.5 write today's lab.  The puzzle of "what is a methodology
that leads to clear writing" is an interesting one.  And you'll note
the current methodology is not optimal.  But the writing is extremely
clear and concrete, at least for the first 4 sections.  (Apologies,
the Sections after that were rushed because I got distracted with lab
side-quests.)

The format is an experiment:
  1. You get a complete walked through example.
  2. There are some pointers for where to go.
  3. Next lab will have a bunch of cards to build out.

The result at the end: you'll be able to run all sorts of interesting
code on these interesting chips.  You might never meet anyone else that
can say the same, which is always cool.


Credit:
  - The initial inspiration for the work was Javier Nieto.  He built a
    Bluetooth lab for 340lx 2025 (by hand) using the vendor firmare
    as a follow-on to the crazy revere engineering he and Ellen Xu did
    for their 140e 2025 final project, manually using oscilliscopes and
    wire traces to reverse engineer the Banana Pi Bluetooth.  I helped
    Javier out a bit with the PL011 (turns out the BCM2835 datasheet
    has an important unknown errata!) and then got sucked into trying
    to do a full Bluetooth and Wifi driver without firmware.  And then
    a year burned up with tokens.

For today, since everyone is busy on mid-terms and mid-term projects
we'll just do a light quick lab:
 - Go through the [README](bluetooth/README.md) and the [Bluetooth Crash course](bluetooth/BLUETOOTH-CRASH-COURSE.md).  Make sure it runs and you understand it.
 - Do some simple modifications to compute more stuff and play with the
   [bluetooth hello world](bluetooth/BLUETOOTH-HELLO-WORLD.md] (apologies: this section needs more editting).
 - If you feel ambitious see what kind of crazy stuff you can figure
   out.  
 - If you get very ambitious: [do Javier Nieto's Bluetooth lab from last year's 340lx](https://github.com/dddrrreee/cs340lx-25aut/tree/main/labs/10-bluetooth)!


### Timeline: Original Bluetooth and Wi-Fi investigation

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
