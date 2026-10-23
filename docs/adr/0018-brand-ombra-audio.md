# 0018 — Brand: Ombra Audio, for both plugins

**Status:** Accepted, 23 Oct 2026 (owner).

**Context:** The desktop plugin (the AU/VST3 test bench) needs a maker name, which hosts like Ableton use to group plugins. Until now Immensio borrowed Resilio's (company "Resilio", manufacturer code `Rslo`). The owner wanted Resilio and Immensio grouped under one maker with a brand of its own. Names explored with the owner (space and air, distance, decay, abstract painting and shadow terms) were checked against existing audio, plugin and Eurorack makers; Halcyon (Halcyon Modular, Halcyon Digital, Origin Effects' pedals) and Far Field (Farfield Audio) were taken; Ombra and Penumbra came back clear apart from a drum sample pack called Ombra.

**Decision:** the maker is **Ombra Audio** (Italian *ombra*, shadow), manufacturer code **`Ombr`**. Immensio keeps plugin code `ImVs`; its bundle ID is `com.OmbraAudio.ImmensioVersio` (`plugin/CMakeLists.txt`, `tools/install_plugin.sh`, `tools/make_release.sh`). Resilio moves to the same maker in its own repo.

**Consequences:**
- A maker code change makes hosts see a new plugin, so settle it once. Immensio had never been installed in Ableton, so it switched freely. Resilio is installed and in the owner's sets: check those before switching it.
- Not a trademark search: check the US/EU/UK registers and the web address before publishing anything under the name.
- Fixed on the way: the install and release scripts used Resilio's plugin code (`RsVs`) for Immensio, and the release script linked Resilio's repo.
