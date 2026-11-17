# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
"""Offline specification-contract checks. `python -m pip install jsonschema` first."""
from pathlib import Path
import copy
import json
import unittest

from jsonschema import Draft202012Validator, FormatChecker, ValidationError

ROOT = Path(__file__).resolve().parents[1]

def schema(name):
    s = json.loads((ROOT / 'schemas' / f'{name}.schema.json').read_text())
    Draft202012Validator.check_schema(s)
    return Draft202012Validator(s, format_checker=FormatChecker())

def example(name):
    return json.loads((ROOT / 'examples' / f'{name}.json').read_text())

class SchemaContractTests(unittest.TestCase):
    def test_all_seven_schemas_are_valid(self):
        for name in ('artifact','event','observation','correlation','reconstruction','claim','verdict'):
            with self.subTest(name=name): schema(name)
    def test_monocular_unknown_range_is_accepted(self):
        schema('reconstruction').validate(example('monocular-unknown-range'))
    def test_monocular_claim_is_accepted(self):
        schema('claim').validate(example('indeterminate-claim'))
    def test_unidentifiable_range_cannot_emit_speed(self):
        value = example('monocular-unknown-range')
        value['speed_m_s']=100000.0
        with self.assertRaises(ValidationError): schema('reconstruction').validate(value)
    def test_missing_falsification_is_rejected(self):
        value=example('indeterminate-claim')
        value['falsifiers']=[]
        with self.assertRaises(ValidationError): schema('claim').validate(value)
    def test_unsigned_unknown_claim_field_is_rejected(self):
        value=example('indeterminate-claim')
        value['secret_new_field']='silent'
        with self.assertRaises(ValidationError): schema('claim').validate(value)

if __name__ == '__main__': unittest.main()
