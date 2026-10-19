# Contact Resolver Interface (experimental)

`host.missionlink_contact_resolver` is a small read-only adapter over the local contact book for future GUI, CLI, and gateway clients. It keeps callers from depending directly on the JSON file format and makes ambiguous labels explicit.

## Example

```python
from host.missionlink_contact_resolver import ContactResolver, ResolutionStatus

resolver = ContactResolver.from_path("contacts.json")
result = resolver.resolve("Vancouver relay")

if result.status is ResolutionStatus.MATCHED:
    contact = result.contact
    print(contact.mesh_number)
elif result.status is ResolutionStatus.AMBIGUOUS:
    # Ask the user to select one of result.matches.
    print("Choose a contact:", [item.mesh_number for item in result.matches])
else:
    print("Contact not found")
```

Resolution rules:

- An exact Mesh Number match wins.
- Otherwise, an exact case-insensitive label match is attempted.
- No match returns `NOT_FOUND`.
- Multiple contacts with the same label return `AMBIGUOUS`; the resolver never silently picks one.
- `search(query)` returns deterministic substring matches by label or Mesh Number.
- Empty or non-string queries raise `ValueError`.

The resolver is read-only and does not launch a subprocess, write files, discover nodes, verify key fingerprints, authorize a service, or send messages. A resolved contact is only a local address-book entry; it does not prove the node is reachable or trusted.

Run tests:

```sh
python -m unittest host.tests.test_missionlink_contact_resolver -v
```
