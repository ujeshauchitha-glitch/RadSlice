# Path forward: money, funding routes, professors, and the three weeks to 30 November 2026

Written 2026-11-10. An assessment, not a plan to hype. Every external fact below names the page it was read from on
that date; anything not so marked is judgement. No names, emails or openings are invented: where a page did not say
something, this file says so.

## 1. Money: what this project can and cannot earn

**Directly: essentially nothing.** A 2,368-weight digit classifier with simulated upsets is not a product, and both
council reviews (`planning/council-verdict.md`, `planning/council-2-direction.md`) agreed that no member would pitch it
for venture money. The honest statement is that the repository is a research artifact whose value is *to the person
who made it*: as evidence of method (preregistration, controls that fail, failures kept, reproducibility) when applying
for a fellowship, an internship, a project position or a thesis slot. Realistic routes and rough odds:

| Route | What it pays | Odds in 6 months, as the author sees them | What moves the odds |
|---|---|---|---|
| Science Academies Summer Research Fellowship 2027 (two months at an Indian lab, stipend and train fare) | a stipend, a host, a guide | the second council put a fellowship-class outcome at roughly even odds *with* a sponsor; without a teacher's recommendation the application cannot be completed at all | a faculty recommender; a clear two-page summary |
| A credited project or thesis with a faculty member at the owner's own university | time, lab access, a recommender, sometimes a vendor board through the faculty channel | good, if asked in person with a specific small request | asking |
| A research internship abroad (NUS, IISc, IITs) via a summer programme, after a professor has seen the work | a stipend for two to three months | low for an unsolicited approach; moderate once a home-university sponsor has written | a sponsor's email, not a cold one |
| A workshop paper (fault tolerance or reliable ML hardware venues) | nothing in money; a citable artifact | moderate once the hardware cross-check exists, low on the simulation alone | the board result, the prior-art reading |
| Vendor donation (AMD or Altera university programmes) | a board, licences | only through faculty; the second council's reading of the programme pages said requests come from faculty | a sponsor |
| Startup, grant from a company, bounty | money | none found; do not pursue | nothing in the next year |

Nothing in this table pays the owner for the work already done. What the work done buys is credibility for the
routes in rows 1 to 4.

## 2. Funding routes, with what was verified

### 2.1 Science Academies Summer Research Fellowship Programme 2027
Read on 2026-11-10 at https://webjapps.ias.ac.in/fellowship2027/ : the programme is run jointly by the Indian Academy
of Sciences, the Indian National Science Academy and the National Academy of Sciences, India; it is two months in
2027 with scientists affiliated with the academies; the page states "The last date for receipt of applications online
is 30 November 2026"; applicants give the email address of a teacher familiar with their work and the academy contacts
that teacher for a recommendation in a prescribed format; results are expected around March to April 2027. The page
does **not** state a year-of-study rule or a marks rule; the full criteria are inside the online form. **Action for
the owner:** open the form, read the eligibility inside it, and decide whether the year of study qualifies. The
write-up field of the form is the place for the one-paragraph question and the repository link.

### 2.2 A faculty sponsor at the owner's university
Needed for everything else: the fellowship recommendation, any vendor donation, any introduction abroad, and any beam
or board time at another institution. The right person is whoever teaches VLSI, embedded systems, FPGA design or
computer architecture; a fault-tolerance interest is ideal but rare. What to bring is in section 3.

### 2.3 Vendor donations and shared facilities
The second council recorded (from the programme pages it saw as snippets) that AMD's university donation programme
takes requests from faculty, that Altera has a limited university programme, and that MeitY's Chips-to-Startup and
ChipIN (C-DAC) offer shared tools and boards to participating institutions. None of these was re-verified for this
file; each is a question for the sponsor, not for the student.

### 2.4 What a funder of any of these pays for
A person with a sponsor and a hardware-validated, open, reproducible result. The repository now has the open and
reproducible parts (`make reproduce`, `docs/audit-2026-11.md`); it does not have the sponsor or the hardware run.

## 3. Approaching professors

### 3.1 What the first email needs (any professor, any country)
- The question in one sentence (the one at the top of `README.md`).
- One result in one sentence, with its limit: "in simulation, voting on accumulators does nothing for corrupted
  shared weights, and only a checksum with a clean reload copy restores accuracy; the reload cost is not modelled."
- One wrong prediction, stated plainly: "ECC alone was predicted to hold at 1e-6 and did not (0.663), because errors
  accumulate over the mission."
- The link to the repository and the two commands (`make test`, `make reproduce`).
- A small, specific ask that costs them under an hour: a recommendation for the fellowship; or 30 minutes to say
  whether the three-way cross-check (software injection, Verilog, board) is worth a student's semester; or whether
  the group has a board a student could use.
- The home-university sponsor's name, if there is one. Without it, expect no reply from abroad; that is normal.
- Nothing about spacecraft, MoS2, novelty, YC or "beam-ready". Nothing longer than 200 words.

### 3.2 NUS: what the pages actually say (read 2026-11-10)
The relevant areas at NUS are split between the School of Computing (architecture, embedded systems) and the
Department of Electrical and Computer Engineering (devices, 2D materials). The pages below were read directly; the
list is not complete and was not filtered by anyone at NUS.

**Re-check on 2026-11-11 (a second pass, fetching each page again).** Confirmed by that fetch: the fellowship page (30 November 2026 deadline, recommendation letters requested by the academies from teachers, selection letters around March-April 2027); Carlson's page and join-us page (Sniper, accelerators, the two open areas, SINGA); Mitra's page (Dean, June 2025 call, Lin-Analyzer); the stale Kumar and Wong pages. **Not re-confirmed, treat as unverified until read in a browser:** the Alioto page (https://www.green-ic.org/ refused the automated fetch with HTTP 406), and the Ang and Thean staff pages (the fetch returned no readable text, so the quoted titles, "Beyond CMOS" phrase and news item could not be checked). No professor's email address or deadline other than the fellowship's appears in this file, and none should be added without reading it from the page.

**Reliable or efficient ML hardware, architecture, embedded systems**

- **Trevor E. Carlson**, Associate Professor, Department of Computer Science, School of Computing, Computer
  Architecture Lab. https://www.comp.nus.edu.sg/~tcarlson/ lists hardware and software design for performance,
  security and efficiency, fast and accurate simulation (the Sniper simulator), and AI accelerators; reliability and
  fault injection are not mentioned. The page https://www.comp.nus.edu.sg/~tcarlson/join-us/ lists two open areas:
  "High-Performance, Green AI Accelerators (PhD, Postdoc, Researchers, Internship openings)" and "Secure Systems (PhD
  openings)", says non-PhD applicants may write directly, and names the Singapore International Graduate Award (SINGA)
  for PhD applicants. **Fit:** the FPGA accelerator and simulation-methodology side, not the reliability question;
  the internship opening is the concrete one. Whether a simulated fault-tolerance study interests the group is not
  stated anywhere on the page.
- **Tulika Mitra**, Dean of the School of Computing, Provost's Chair Professor of Computer Science, Embedded Computing
  Lab (eCO). https://www.comp.nus.edu.sg/~tulika/ states hardware-software co-design of "smart, energy-efficient,
  safety-critical embedded computing systems", software-defined accelerators, FPGA performance analysis
  (Lin-Analyzer) and edge ML systems; a June 2025 call for research fellows, PhD students and research assistants with
  systems expertise (compilers, design automation, accelerator architecture, ML systems) on two funded programmes.
  Fault tolerance is not mentioned. **Fit:** embedded accelerators and edge ML; a Dean is unlikely to answer a
  student's cold email, so a sponsor's introduction matters most here.
- **Massimo Alioto**, Green IC group, Department of Electrical and Computer Engineering. https://www.green-ic.org/
  states energy-efficient integrated circuits, IoT and battery-less systems, energy-quality scalable systems and
  hardware security, with open positions and a "Theses, internships, visits" section. Reliability, error resilience
  and ML hardware are not discussed on the page beyond the word "Resilient" in the group's name. **Fit:** weak on
  the question; strong if the work ever becomes a measured-cost study of protection logic on silicon.
- Pages that looked relevant from search results but are stale and should not be used as leads: the NUS ECE page of
  Akash Kumar (https://www.ece.nus.edu.sg/stfpage/eleak/) lists embedded multiprocessor systems and a 2014 job call;
  the NUS Computing page of Weng-Fai Wong (https://www.comp.nus.edu.sg/~wongwf/) says it was last updated in 2014.
  Neither page mentions fault tolerance.

**2D-materials electronics**

- **Ang Kah Wee**, Professor and Area Director (Microelectronics Technology and Devices), Department of Electrical and
  Computer Engineering. https://cde.nus.edu.sg/ece/staff/ang-kah-wee/ lists nano-electronics and nano-photonics
  technologies, a CMOS-scaling and silicon-photonics industry background, and an editorial role; the biography text on
  the page still says "Assistant Professor" while the header says Professor, so the page is partly out of date. A
  department news item found by search (https://cde.nus.edu.sg/ece/news/nus-ece-research-led-by-prof-ang-kah-wee-recognised-as-nanoscale-horizons-2025-outstanding-review/)
  concerns a 2025 review on 2D-materials hardware for in-sensor computing; the news page itself could not be read here,
  so only its title is cited. The staff page says nothing about students or openings. **Fit:** Track B's device-data
  question (yield variation across dies, failure modes of failing circuits), which is what `planning/collaboration-2d.md`
  asks for.
- **Aaron Voon-Yew Thean**, Professor of Electrical and Computer Engineering; the page lists him as Deputy President
  (Academic Affairs) and Provost. https://cde.nus.edu.sg/ece/staff/aaron-voon-yew-thean/ describes advanced device
  technology at IMEC including "Beyond CMOS logic nano-device architectures based on Spintronics and 2-D materials" and
  design-technology co-optimisation. No statement about students or openings. **Fit:** the topic, but a Provost will
  not be reached by a student's email; relevant only if a sponsor knows the group.
- The NUS ECE research-areas page https://cde.nus.edu.sg/ece/research/research-areas/ lists "Integrated Circuits &
  Embedded Systems" and "Microelectronic Technologies & Devices" among seven areas; the member lists behind them did
  not load as text here and should be read in a browser. The Centre for Advanced 2D Materials (graphene.nus.edu.sg)
  could not be reached from this machine on 2026-11-10; a search result describing its About page said it has four
  research groups, which could not be confirmed.

**What was not found at NUS:** a group whose stated topic is fault tolerance of hardware or of ML accelerators. The
closest fits are accelerator and simulation groups (Carlson, Mitra) on the Track A side and device groups (Ang, Thean)
on the Track B side. Any email should say which side it is about and should not pretend the group works on
radiation or reliability if its page does not say so.

### 3.3 IISc and the IITs
Not researched for this file. The same rule applies: read the group page, cite one of its papers in the email, ask
one small thing. The fellowship (section 2.1) is the natural route to an Indian lab because the host is assigned
through the programme and the student does not need to be known first.

## 4. Three weeks to 30 November 2026
The second council's "before 30 November" list is kept; the audit found the repository already reproduces, so the
time goes to the sponsor and the reading rather than to code.

| Week | Do | Done when |
|---|---|---|
| 1 (to 16 Nov) | Ask one faculty member in person for a fellowship recommendation and for a credited project, with the README, `make reproduce` output and this file printed. Open the fellowship form and read the eligibility inside it. | a yes or a no from one person; eligibility known |
| 1 to 2 | Read the six arXiv papers named in `planning/council-2-direction.md` and answer its five prior-art questions in a new `docs/prior-art.md` (one paragraph each, with what RadSlice does differently or not at all). | the file exists and the README's "no novelty claimed" is either kept or sharpened |
| 2 (to 23 Nov) | Write the two-page summary (question, the three headline results with limits, the wrong predictions, the cross-check proposal, the ask). Submit the fellowship application by about 25 Nov so the recommender has time. | submitted |
| 3 (to 30 Nov) | Only if the sponsor said yes: install MRFI and run the G20 fault list through it on the same network; record agreement or disagreement with the Verilog in `docs/results/`. Otherwise: fix the submodule pin (section 5) and stop. | a result file either way, or the pin fixed |

Not in the three weeks: the board run (needs the cable and a bench), any email abroad (needs the sponsor), any
claim of novelty (needs the reading).

## 5. What raises or lowers the odds
**Raises:** a sponsor's name on the application; a `docs/prior-art.md` that cites the papers and states the gap in one
sentence; MRFI and the Verilog agreeing (or a recorded disagreement with a cause); the DE2-115 running the network
bit-exact even without faults; one professor replying with a question rather than a form answer.

**Lowers:** no recommender by 23 November (the fellowship then cannot be submitted); the prior-art reading finding
that the MRFI-versus-hardware check has been published already (then the question changes to what that paper left
out, and the file says so); the submodule pin becoming unfetchable before it is tagged (the C++ half of the record
then cannot be rebuilt); any document reviving the spacecraft, MoS2-is-rad-hard or YC framing.

## 6. Decisions only the owner can make
1. Who the faculty sponsor is, and whether to ask this week.
2. The year of study, which decides fellowship eligibility (the page does not state the rule; the form does).
3. Whether to install MRFI now (the plan's week 3) or after the application.
4. Whether to buy the USB-Blaster cable and book bench time, which is the only path from "compiles" to "ran".
5. Whether MIT is the licence (added on 2026-11-10 on instruction; change it if the owner wants another).
6. Whether to push a tag at commit `39d3d95` in `riscv-emu` so the golden-model pin stays fetchable.
