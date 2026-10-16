import unittest

from radslice import assumptions
from radslice.params import load_params


class AssumptionsRegister(unittest.TestCase):
    def test_committed_document_matches_the_parameters(self):
        committed = assumptions.DOC_PATH.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertEqual(committed, assumptions.render(),
                         "docs/assumptions.md is stale: run python -m radslice.assumptions --write")

    def test_every_parameter_appears(self):
        text = assumptions.render()
        for name in load_params():
            self.assertIn(f"`{name}`", text)

    def test_states_that_nothing_is_sourced_while_all_tier_c(self):
        params = load_params()
        if all(p.tier == "C" for p in params.values()):
            self.assertIn("No parameter has been checked against a primary source", assumptions.render())


if __name__ == "__main__":
    unittest.main()
