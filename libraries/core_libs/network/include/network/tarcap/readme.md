### Multiple taraxa capabilities support
- Create new folder, e.g. `capability_v1`
- Create new `TaraxaCapability` class derived from `TaraxaCapabiliyyBase` in new namespace, e.g.:


    # Create clang profile 
    namespace taraxa::network::tarcap::v1 {

    class TaraxaCapability : public taraxa::network::tarcap::TaraxaCapabilityBase {
      ...
    }

    }
- Derive new packet handlers with different logic than the original one.
- 
  `!!! Important:` These handlers must be
  directly on indirectly derived from the latest packets handlers, which are inside
  `network/tarcap/capability_latest` folder, otherwise network class would not work properly
  and project would not compile