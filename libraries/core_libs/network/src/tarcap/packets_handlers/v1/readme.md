### Multiple taraxa capabilities support
- Derive new packet handlers with different logic than the original ones.
-
`!!! Important:` These handlers must be
directly on indirectly derived from the latest packets handlers, which are inside
`network/tarcap/packets_handlers/latest/` folder, otherwise network class would not work properly
