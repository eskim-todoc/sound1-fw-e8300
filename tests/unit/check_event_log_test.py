"""Reject truncated, duplicate and self-edited audit evidence without hardware."""
import unittest
from check_event_log import check_records, EXPECTED_FIELDS


def valid_records():
    return [{"case": case, "field": field, "expected": expected,
             "actual": 0 if case in (24, 25) else expected}
            for (case, field), expected in EXPECTED_FIELDS.items()]


class EvidenceTest(unittest.TestCase):
    def test_complete_records_keep_two_failures(self):
        self.assertEqual(len(check_records(valid_records())), 2)

    def test_one_record_per_case_is_not_complete(self):
        keep = {r["case"]: r for r in valid_records()}
        with self.assertRaises(AssertionError):
            check_records(list(keep.values()))

    def test_duplicate_field_or_oracle_change_is_rejected(self):
        records = valid_records()
        for invalid in (records + [records[0]], [records[0], *records[2:]],
                        [{**records[0], "actual": 7, "expected": 7}, *records[1:]]):
            with self.assertRaises(AssertionError):
                check_records(invalid)


if __name__ == "__main__":
    unittest.main()
