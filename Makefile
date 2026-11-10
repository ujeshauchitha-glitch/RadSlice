# RadSlice: the targets a reviewer needs. GNU make (on Windows: mingw32-make, or run the commands by hand).
#
#   make test        the Python tests (about 5 s)
#   make reproduce   regenerate the headline results: G19 step 4 (accuracy against upset rate, five
#                    schemes) and G20 (weight-memory protection, preregistered); about 2 minutes
#   make assumptions regenerate docs/assumptions.md from config/params.toml
#   make check       test + reproduce, then show whether the committed result files changed
#
# Only Python (>= 3.11) and requirements.txt are needed for these. The C++ model, the RISC-V workloads
# and the Verilog checks have their own commands; see README.md.

# The interpreter that has numpy, scikit-learn and matplotlib. Override if the first `python` on PATH is a
# different one (on Windows with MSYS2 first on PATH, its python lacks numpy): make PYTHON=py reproduce
PYTHON ?= python

.PHONY: test reproduce assumptions check deps

deps:
	$(PYTHON) -m pip install -r requirements.txt

test:
	cd python && $(PYTHON) -m unittest discover -s tests

reproduce:
	$(PYTHON) scripts/g19_reference.py
	$(PYTHON) scripts/g19_campaign.py
	$(PYTHON) scripts/g19_report.py
	$(PYTHON) scripts/g20_weight_protection.py
	@echo "regenerated: results/g19_baseline.md results/g19_campaign.{csv,md,png,svg} results/g20_weight_protection.{csv,md}"

assumptions:
	cd python && $(PYTHON) -m radslice.assumptions --write

check: test reproduce
	git status --short results/ docs/assumptions.md
	@echo "(an empty list above means the committed results reproduced exactly; g19_campaign.svg differs only by timestamp and element ids)"
