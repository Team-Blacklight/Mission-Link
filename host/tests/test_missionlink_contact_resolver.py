import tempfile
import unittest
from pathlib import Path

from host.missionlink_contacts import Contact, ContactBook
from host.missionlink_contact_resolver import ContactResolver, ResolutionStatus

MISSION = "(777)-111-2222"
VANCOUVER = "(777)-482-1937"


class ContactResolverTests(unittest.TestCase):
    def setUp(self):
        book = ContactBook()
        book.upsert(Contact(MISSION, "Mission base", node_id="mission_node"))
        book.upsert(Contact(VANCOUVER, "Vancouver relay", node_id="vancouver_node"))
        self.book = book
        self.resolver = ContactResolver(book)

    def test_resolves_exact_mesh_number_before_label(self):
        result = self.resolver.resolve(VANCOUVER)
        self.assertEqual(result.status, ResolutionStatus.MATCHED)
        self.assertEqual(result.contact.mesh_number, VANCOUVER)

    def test_resolves_label_case_insensitively(self):
        result = self.resolver.resolve("vAnCoUvEr ReLaY")
        self.assertEqual(result.status, ResolutionStatus.MATCHED)
        self.assertEqual(result.contact.mesh_number, VANCOUVER)

    def test_not_found_is_explicit(self):
        result = self.resolver.resolve("Unknown relay")
        self.assertEqual(result.status, ResolutionStatus.NOT_FOUND)
        self.assertIsNone(result.contact)
        self.assertEqual(result.matches, ())

    def test_duplicate_labels_are_ambiguous_not_arbitrarily_selected(self):
        self.book.upsert(Contact("(777)-333-4444", "Mission base"))
        result = self.resolver.resolve("MISSION BASE")
        self.assertEqual(result.status, ResolutionStatus.AMBIGUOUS)
        self.assertIsNone(result.contact)
        self.assertEqual(
            {contact.mesh_number for contact in result.matches},
            {MISSION, "(777)-333-4444"},
        )

    def test_search_is_case_insensitive_and_stably_ordered(self):
        matches = self.resolver.search("RELAY")
        self.assertEqual([contact.mesh_number for contact in matches], [VANCOUVER])

    def test_empty_queries_are_rejected(self):
        for query in ("", "   ", None, 123):
            with self.subTest(query=query), self.assertRaises(ValueError):
                self.resolver.resolve(query)
        with self.assertRaises(ValueError):
            self.resolver.search(" ")

    def test_from_path_loads_and_validates_book(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "contacts.json"
            self.book.save(path)
            resolver = ContactResolver.from_path(path)
            self.assertEqual(resolver.resolve("Mission base").contact.mesh_number, MISSION)


if __name__ == "__main__":
    unittest.main()
