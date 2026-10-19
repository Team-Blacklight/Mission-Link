"""Reusable, side-effect-free contact lookup for MissionLink host clients.

This layer resolves local labels to Mesh Numbers. It does not perform discovery,
authentication, key verification, or message delivery.
"""
from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from os import PathLike
from typing import TypeAlias

from host.missionlink_contacts import Contact, ContactBook, ContactBookError

PathValue: TypeAlias = str | PathLike[str]


class ResolutionStatus(str, Enum):
    MATCHED = "matched"
    NOT_FOUND = "not_found"
    AMBIGUOUS = "ambiguous"


@dataclass(frozen=True)
class ContactResolution:
    """Result that forces callers to handle missing or ambiguous labels."""

    query: str
    status: ResolutionStatus
    contact: Contact | None = None
    matches: tuple[Contact, ...] = ()

    def __post_init__(self) -> None:
        if self.status is ResolutionStatus.MATCHED:
            if self.contact is None or self.matches:
                raise ValueError("invalid_matched_resolution")
        elif self.status is ResolutionStatus.NOT_FOUND:
            if self.contact is not None or self.matches:
                raise ValueError("invalid_not_found_resolution")
        elif self.status is ResolutionStatus.AMBIGUOUS:
            if self.contact is not None or len(self.matches) < 2:
                raise ValueError("invalid_ambiguous_resolution")


class ContactResolver:
    """Read-only lookup facade shared by CLI, GUI, and future gateway clients."""

    def __init__(self, book: ContactBook) -> None:
        if not isinstance(book, ContactBook):
            raise TypeError("book_must_be_contact_book")
        self._book = book

    @classmethod
    def from_path(cls, path: PathValue) -> "ContactResolver":
        """Load and validate a contact book from disk."""
        return cls(ContactBook.load(path))

    def resolve(self, query: str) -> ContactResolution:
        """Resolve an exact Mesh Number or case-insensitive exact label.

        Mesh Number wins when supplied exactly. Labels are not assumed unique:
        duplicate labels produce AMBIGUOUS rather than choosing arbitrarily.
        """
        if not isinstance(query, str) or not query.strip():
            raise ValueError("query_must_not_be_empty")
        cleaned = query.strip()
        direct = self._book.get(cleaned)
        if direct is not None:
            return ContactResolution(cleaned, ResolutionStatus.MATCHED, contact=direct)

        folded = cleaned.casefold()
        matches = tuple(
            contact for contact in self._book.list_contacts()
            if contact.label.casefold() == folded
        )
        if len(matches) == 1:
            return ContactResolution(cleaned, ResolutionStatus.MATCHED, contact=matches[0])
        if len(matches) > 1:
            return ContactResolution(cleaned, ResolutionStatus.AMBIGUOUS, matches=matches)
        return ContactResolution(cleaned, ResolutionStatus.NOT_FOUND)

    def search(self, query: str) -> tuple[Contact, ...]:
        """Return deterministic substring matches across label and Mesh Number."""
        if not isinstance(query, str) or not query.strip():
            raise ValueError("query_must_not_be_empty")
        folded = query.strip().casefold()
        return tuple(
            contact for contact in self._book.list_contacts()
            if folded in contact.label.casefold() or folded in contact.mesh_number.casefold()
        )
