/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "TARAXA", "index.html", [
    [ "Introducing Taraxa", "index.html", [
      [ "Whitepaper", "index.html#autotoc_md1", null ],
      [ "Quickstart", "index.html#autotoc_md2", null ],
      [ "Downloading", "index.html#autotoc_md3", null ],
      [ "Building", "index.html#autotoc_md6", null ],
      [ "Running", "index.html#autotoc_md7", null ],
      [ "Contributing", "index.html#autotoc_md10", null ],
      [ "Useful doc", "index.html#autotoc_md11", null ]
    ] ],
    [ "Multiple taraxa capabilities support", "md_libraries_core_libs_network_src_tarcap_packets_handlers_latest_readme.html", null ],
    [ "Building taraxa-node", "md_doc_building.html", [
      [ "Building on Ubuntu 24.04", "md_doc_building.html#autotoc_md29", [
        [ "Compile-Time Options (cmake)", "md_doc_building.html#autotoc_md27", [
          [ "Docker image", "index.html#autotoc_md4", null ],
          [ "Ubuntu binary", "index.html#autotoc_md5", null ],
          [ "Inside docker image", "index.html#autotoc_md8", null ],
          [ "Pre-built binary or manual build:", "index.html#autotoc_md9", null ],
          [ "CMAKE_BUILD_TYPE=[Release/Debug/RelWithDebInfo]", "md_doc_building.html#autotoc_md28", null ]
        ] ],
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md30", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md31", null ],
        [ "Compile", "md_doc_building.html#autotoc_md32", null ]
      ] ],
      [ "Building on Ubuntu 22.04", "md_doc_building.html#autotoc_md33", [
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md34", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md35", null ],
        [ "Compile", "md_doc_building.html#autotoc_md36", null ]
      ] ],
      [ "Building on Ubuntu 20.04", "md_doc_building.html#autotoc_md37", [
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md38", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md39", null ],
        [ "Compile", "md_doc_building.html#autotoc_md40", null ]
      ] ],
      [ "Building on MacOS", "md_doc_building.html#autotoc_md41", [
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md42", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md43", null ],
        [ "Compile", "md_doc_building.html#autotoc_md44", null ],
        [ "Known issues", "md_doc_building.html#autotoc_md45", [
          [ "Issues with conan cache", "md_doc_building.html#autotoc_md46", null ]
        ] ]
      ] ],
      [ "Building on M1 Macs for x86_64 with Rosetta2", "md_doc_building.html#autotoc_md47", [
        [ "Install Rosetta2", "md_doc_building.html#autotoc_md48", null ],
        [ "Run an x86_64 session", "md_doc_building.html#autotoc_md49", null ],
        [ "Install Homebrew", "md_doc_building.html#autotoc_md50", null ],
        [ "Install dependencies", "md_doc_building.html#autotoc_md51", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md52", null ],
        [ "Compile", "md_doc_building.html#autotoc_md53", null ]
      ] ],
      [ "Run", "md_doc_building.html#autotoc_md54", [
        [ "Running tests", "md_doc_building.html#autotoc_md55", null ],
        [ "Running taraxa-node", "md_doc_building.html#autotoc_md56", null ]
      ] ],
      [ "Building using \"taraxa-builder\" docker image", "md_doc_building.html#autotoc_md57", null ]
    ] ],
    [ "C++ Best Practices Guidelines", "md_doc_coding_practices.html", [
      [ "The structure of this document", "md_doc_coding_practices.html#autotoc_md59", [
        [ "<a name=\"compiletimechecking\"></a> 1. Prefer compile-time checking to run-time checking", "md_doc_coding_practices.html#autotoc_md60", null ],
        [ "<a name=\"avoidnonconst\"></a> 2. Avoid non-<tt>const</tt> global variables", "md_doc_coding_practices.html#autotoc_md64", null ],
        [ "<a name=\"ruleofzero\"></a> 3. If you can avoid defining default operations, do", "md_doc_coding_practices.html#autotoc_md71", null ],
        [ "<a name=\"ruleoffive\"></a> 4. If you define or <tt>=delete</tt> any default operation, define or <tt>=delete</tt> them all", "md_doc_coding_practices.html#autotoc_md76", null ],
        [ "<a name=\"smartptr\"></a> 5. Never transfer ownership by a raw pointer (<tt>T*</tt>) or reference (<tt>T&</tt>)", "md_doc_coding_practices.html#autotoc_md85", null ],
        [ "<a name=\"usingsmartptr\"></a> 6. Choose appropriate smart pointer or why we have more smart pointers?", "md_doc_coding_practices.html#autotoc_md90", null ],
        [ "<a name=\"singlealloc\"></a> 7. Perform at most one explicit resource allocation in a single expression statement", "md_doc_coding_practices.html#autotoc_md94", null ],
        [ "<a name=\"passsmartptr\"></a> 8. Take smart pointers as parameters only to explicitly express lifetime semantics", "md_doc_coding_practices.html#autotoc_md98", null ],
        [ "<a name=\"varlimitscope\"></a> 9. Declare names in for-statement initializers and conditions to limit scope", "md_doc_coding_practices.html#autotoc_md103", null ],
        [ "<a name=\"varrecycle\"></a> 10. Don't use a variable for two unrelated purposes", "md_doc_coding_practices.html#autotoc_md109", null ],
        [ "<a name=\"macrosconsts\"></a> 11. Don't use macros for constants or \"functions\"", "md_doc_coding_practices.html#autotoc_md114", null ],
        [ "<a name=\"magicconstants\"></a> 12. Avoid \"magic constants\"; use symbolic constants", "md_doc_coding_practices.html#autotoc_md118", null ],
        [ "<a name=\"nullptr\"></a> 13. Use <tt>nullptr</tt> rather than <tt>0</tt> or <tt>NULL</tt>", "md_doc_coding_practices.html#autotoc_md122", null ],
        [ "<a name=\"construct\"></a> 14. Use the <tt>T{e}</tt>notation for construction", "md_doc_coding_practices.html#autotoc_md126", null ],
        [ "<a name=\"referencecapture\"></a> 15. Prefer capturing by reference in lambdas that will be used locally, including passed to algorithms", "md_doc_coding_practices.html#autotoc_md132", null ],
        [ "<a name=\"valuecapture\"></a> 16. Avoid capturing by reference in lambdas that will be used nonlocally, including returned, stored on the heap, or passed to another thread", "md_doc_coding_practices.html#autotoc_md139", null ],
        [ "<a name=\"thiscapture\"></a> 17. If you capture <tt>this</tt>, capture all variables explicitly (no default capture)", "md_doc_coding_practices.html#autotoc_md144", null ],
        [ "<a name=\"defaultctor\"></a> 18. Don't define a default constructor that only initializes data members; use in-class member initializers instead", "md_doc_coding_practices.html#autotoc_md149", null ],
        [ "<a name=\"explicitctor\"></a> 19. By default, declare single-argument constructors explicit", "md_doc_coding_practices.html#autotoc_md154", null ],
        [ "<a name=\"alwaysinitialize\"></a> 20. Always initialize an object", "md_doc_coding_practices.html#autotoc_md159", null ],
        [ "<a name=\"lambdainit\"></a> 21. Use lambdas for complex initialization, especially of <tt>const</tt> variables", "md_doc_coding_practices.html#autotoc_md170", null ],
        [ "<a name=\"orderctor\"></a> 22. Define and initialize member variables in the order of member declaration", "md_doc_coding_practices.html#autotoc_md176", null ],
        [ "<a name=\"inclassinitializer\"></a> 23. Prefer in-class initializers to member initializers in constructors for constant initializers", "md_doc_coding_practices.html#autotoc_md180", null ],
        [ "<a name=\"initializetoassignment\"></a> 24. Prefer initialization to assignment in constructors", "md_doc_coding_practices.html#autotoc_md185", null ],
        [ "<a name=\"castsnamed\"></a> 25. If you must use a cast, use a named cast", "md_doc_coding_practices.html#autotoc_md189", null ],
        [ "<a name=\"castsconst\"></a> 26. Don't cast away <tt>const</tt>", "md_doc_coding_practices.html#autotoc_md195", null ],
        [ "<a name=\"symmetric\"></a> 27. Use nonmember functions for symmetric operators", "md_doc_coding_practices.html#autotoc_md202", null ]
      ] ]
    ] ],
    [ "Contributing Guide", "md_doc_contributing.html", null ],
    [ "Doxygen", "md_doc_doxygen.html", [
      [ "Installing dependencies", "md_doc_doxygen.html#autotoc_md208", [
        [ "Ubuntu", "md_doc_doxygen.html#autotoc_md209", null ],
        [ "MacOS", "md_doc_doxygen.html#autotoc_md210", null ]
      ] ],
      [ "Generating documentation", "md_doc_doxygen.html#autotoc_md211", null ],
      [ "Commenting", "md_doc_doxygen.html#autotoc_md212", [
        [ "Basic example", "md_doc_doxygen.html#autotoc_md213", null ],
        [ "Grouping to modules", "md_doc_doxygen.html#autotoc_md214", null ],
        [ "Graphs", "md_doc_doxygen.html#autotoc_md215", null ]
      ] ]
    ] ],
    [ "EVM incompatibilities", "md_doc_evm_incompatibilities.html", [
      [ "Unsupported EIPs", "md_doc_evm_incompatibilities.html#autotoc_md217", null ],
      [ "Latest supported solc version", "md_doc_evm_incompatibilities.html#autotoc_md218", null ],
      [ "go-ethereum library", "md_doc_evm_incompatibilities.html#autotoc_md219", null ]
    ] ],
    [ "Git-flow Guide", "md_doc_git_practices.html", [
      [ "Branch naming conventions", "md_doc_git_practices.html#autotoc_md220", null ],
      [ "Main branches", "md_doc_git_practices.html#autotoc_md221", null ],
      [ "Supporting branches", "md_doc_git_practices.html#autotoc_md222", [
        [ "Standard Feature branches", "md_doc_git_practices.html#autotoc_md223", null ],
        [ "Long-term Feature branches", "md_doc_git_practices.html#autotoc_md224", null ],
        [ "Hotfix branches", "md_doc_git_practices.html#autotoc_md225", null ],
        [ "Release branches", "md_doc_git_practices.html#autotoc_md226", null ]
      ] ],
      [ "Branches Cleaning", "md_doc_git_practices.html#autotoc_md227", null ],
      [ "PR merging & Code reviews", "md_doc_git_practices.html#autotoc_md228", null ],
      [ "Commit message conventions", "md_doc_git_practices.html#autotoc_md229", null ],
      [ "Automatic github issues linking", "md_doc_git_practices.html#autotoc_md233", null ],
      [ "Example", "md_doc_git_practices.html#autotoc_md234", null ]
    ] ],
    [ "Quickstart Guide", "md_doc_quickstart_guide.html", [
      [ "Taraxa docker image", "md_doc_quickstart_guide.html#autotoc_md243", [
        [ "Pre-requisites", "md_doc_quickstart_guide.html#autotoc_md236", [
          [ "\"type\" must be one of the following mentioned below!", "md_doc_git_practices.html#autotoc_md230", null ],
          [ "\"scope\" is optional", "md_doc_git_practices.html#autotoc_md231", null ],
          [ "\"subject\"", "md_doc_git_practices.html#autotoc_md232", null ],
          [ "Used git flow:", "md_doc_git_practices.html#autotoc_md235", null ],
          [ "MANDATORY PORT", "md_doc_quickstart_guide.html#autotoc_md237", null ],
          [ "OPTIONAL PORTS", "md_doc_quickstart_guide.html#autotoc_md238", null ]
        ] ],
        [ "Config", "md_doc_quickstart_guide.html#autotoc_md239", [
          [ "Param1", "md_doc_quickstart_guide.html#autotoc_md240", null ],
          [ "Param2", "md_doc_quickstart_guide.html#autotoc_md241", null ],
          [ "...", "md_doc_quickstart_guide.html#autotoc_md242", null ],
          [ "taraxa-builder:latest", "md_doc_quickstart_guide.html#autotoc_md244", null ],
          [ "taraxa-node:latest", "md_doc_quickstart_guide.html#autotoc_md245", null ]
        ] ]
      ] ]
    ] ],
    [ "Standard release cycle", "md_doc_release_cycle.html", [
      [ "Release cycle phases", "md_doc_release_cycle.html#autotoc_md247", [
        [ "Phase 1 - active development of new features", "md_doc_release_cycle.html#autotoc_md248", null ],
        [ "Phase 2 - alpha testing (internal)", "md_doc_release_cycle.html#autotoc_md249", null ],
        [ "Phase 3 - beta testing (public)", "md_doc_release_cycle.html#autotoc_md250", null ],
        [ "Phase 4 - Mainnet release", "md_doc_release_cycle.html#autotoc_md251", null ]
      ] ],
      [ "Ad-hoc releases with bug fixes", "md_doc_release_cycle.html#autotoc_md252", null ]
    ] ],
    [ "Rewards distribution algorithm", "md_doc_rewards_distribution.html", [
      [ "Glossary", "md_doc_rewards_distribution.html#autotoc_md254", null ],
      [ "Rewards sources", "md_doc_rewards_distribution.html#autotoc_md255", null ],
      [ "Rewards distribution", "md_doc_rewards_distribution.html#autotoc_md256", [
        [ "Beneficial work in network", "md_doc_rewards_distribution.html#autotoc_md257", null ],
        [ "Newly created tokens:", "md_doc_rewards_distribution.html#autotoc_md258", null ],
        [ "Included transactions fees:", "md_doc_rewards_distribution.html#autotoc_md259", null ]
      ] ],
      [ "Validators statistics", "md_doc_rewards_distribution.html#autotoc_md260", null ],
      [ "Example:", "md_doc_rewards_distribution.html#autotoc_md261", [
        [ "DAG structure:", "md_doc_rewards_distribution.html#autotoc_md262", null ],
        [ "PBFT block", "md_doc_rewards_distribution.html#autotoc_md263", null ],
        [ "Statistics", "md_doc_rewards_distribution.html#autotoc_md264", null ],
        [ "Rewards", "md_doc_rewards_distribution.html#autotoc_md265", [
          [ "DAG blocks rewards", "md_doc_rewards_distribution.html#autotoc_md266", null ],
          [ "PBFT proposer reward", "md_doc_rewards_distribution.html#autotoc_md267", null ],
          [ "PBFT voters reward", "md_doc_rewards_distribution.html#autotoc_md268", null ]
        ] ]
      ] ]
    ] ],
    [ "Taraxa RPC", "md_doc__r_p_c.html", [
      [ "Ethereum compatibility", "md_doc__r_p_c.html#autotoc_md270", [
        [ "Quirks", "md_doc__r_p_c.html#autotoc_md271", null ],
        [ "Not implemented", "md_doc__r_p_c.html#autotoc_md272", null ]
      ] ],
      [ "Taraxa specific methods", "md_doc__r_p_c.html#autotoc_md273", [
        [ "taraxa_protocolVersion", "md_doc__r_p_c.html#autotoc_md274", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md275", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md276", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md277", null ]
        ] ],
        [ "taraxa_getVersion", "md_doc__r_p_c.html#autotoc_md278", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md279", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md280", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md281", null ]
        ] ],
        [ "taraxa_getDagBlockByHash", "md_doc__r_p_c.html#autotoc_md282", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md283", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md284", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md285", null ]
        ] ],
        [ "taraxa_getDagBlockByLevel", "md_doc__r_p_c.html#autotoc_md286", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md287", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md288", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md289", null ]
        ] ],
        [ "taraxa_dagBlockLevel", "md_doc__r_p_c.html#autotoc_md290", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md291", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md292", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md293", null ]
        ] ],
        [ "taraxa_dagBlockPeriod", "md_doc__r_p_c.html#autotoc_md294", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md295", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md296", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md297", null ]
        ] ],
        [ "taraxa_getScheduleBlockByPeriod", "md_doc__r_p_c.html#autotoc_md298", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md299", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md300", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md301", null ]
        ] ],
        [ "taraxa_pbftBlockHashByPeriod", "md_doc__r_p_c.html#autotoc_md302", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md303", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md304", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md305", null ]
        ] ],
        [ "taraxa_getConfig", "md_doc__r_p_c.html#autotoc_md306", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md307", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md308", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md309", null ]
        ] ],
        [ "taraxa_getChainStats", "md_doc__r_p_c.html#autotoc_md310", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md311", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md312", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md313", null ]
        ] ],
        [ "taraxa_yield", "md_doc__r_p_c.html#autotoc_md314", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md315", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md316", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md317", null ]
        ] ],
        [ "taraxa_totalSupply", "md_doc__r_p_c.html#autotoc_md318", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md319", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md320", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md321", null ]
        ] ],
        [ "taraxa_getPillarBlockData", "md_doc__r_p_c.html#autotoc_md322", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md323", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md324", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md325", null ]
        ] ]
      ] ],
      [ "Test API", "md_doc__r_p_c.html#autotoc_md326", [
        [ "get_sortition_change", "md_doc__r_p_c.html#autotoc_md327", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md328", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md329", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md330", null ]
        ] ],
        [ "send_coin_transaction", "md_doc__r_p_c.html#autotoc_md331", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md332", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md333", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md334", null ]
        ] ],
        [ "send_coin_transactions", "md_doc__r_p_c.html#autotoc_md335", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md336", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md337", null ]
        ] ],
        [ "get_account_address", "md_doc__r_p_c.html#autotoc_md338", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md339", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md340", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md341", null ]
        ] ],
        [ "get_peer_count", "md_doc__r_p_c.html#autotoc_md342", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md343", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md344", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md345", null ]
        ] ],
        [ "get_node_status", "md_doc__r_p_c.html#autotoc_md346", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md347", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md348", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md349", null ]
        ] ],
        [ "get_all_nodes", "md_doc__r_p_c.html#autotoc_md350", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md351", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md352", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md353", null ]
        ] ]
      ] ],
      [ "Debug API", "md_doc__r_p_c.html#autotoc_md354", [
        [ "debug_getPeriodTransactionsWithReceipts", "md_doc__r_p_c.html#autotoc_md355", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md356", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md357", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md358", null ]
        ] ],
        [ "debug_getPeriodDagBlocks", "md_doc__r_p_c.html#autotoc_md359", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md360", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md361", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md362", null ]
        ] ],
        [ "debug_getPreviousBlockCertVotes", "md_doc__r_p_c.html#autotoc_md363", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md364", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md365", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md366", null ]
        ] ]
      ] ]
    ] ],
    [ "readme", "md_doc_uml_readme.html", null ],
    [ "Todo List", "todo.html", null ],
    [ "Modules", "modules.html", "modules" ],
    [ "Namespace Members", "namespacemembers.html", [
      [ "All", "namespacemembers.html", "namespacemembers_dup" ],
      [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
      [ "Variables", "namespacemembers_vars.html", null ],
      [ "Typedefs", "namespacemembers_type.html", null ],
      [ "Enumerations", "namespacemembers_enum.html", null ],
      [ "Enumerator", "namespacemembers_eval.html", null ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Functions", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ],
    [ "Examples", "examples.html", "examples" ]
  ] ]
];

var NAVTREEINDEX =
[
"_a_e_s_8cpp.html",
"_log_8h.html#aae00e571ffe0cad6139ccbde17a26e16",
"class_server_interface.html",
"classdev_1_1_r_l_p_stream.html#ad8b6ebd738c4eec7f4ded266489fe994",
"classdev_1_1p2p_1_1_node_table.html#aee81c2ab6eac0d68f039f0aa338e0e25",
"classgraphql_1_1taraxa_1_1_dag_block.html#a62db2b51be5f3c8941927e095d7b9076",
"classtaraxa_1_1_db_storage.html#a8b2b4952e1b11b991b8c06dbff8bb33e",
"classtaraxa_1_1_slashing_manager.html#a2a34fdd161f25228ea003d9f2cef7227",
"classtaraxa_1_1net_1_1_eth_face.html#a194b9a206ca5e3a3d52b592884987c11",
"classtaraxa_1_1net_1_1_test_face.html#a408774c2df482b4dac82bb6e9823218b",
"classtaraxa_1_1network_1_1tarcap_1_1_get_pillar_votes_bundle_packet_handler.html",
"classtaraxa_1_1network_1_1tarcap_1_1_transaction_packet_handler.html#a708f7675fb0bf1212a45254f148a35e7",
"classtaraxa_1_1util_1_1_thread_pool.html#af9a538ba805c416624b2d634c81f04aa",
"dir_76cd5f7139c2bdac0e56034df35064bd.html",
"group___d_a_g.html#a1a957529304d407e997ca611b4530ab7",
"group___final_chain.html#a4655fb5d4538ce7a5840214f322eb37e",
"group___p_b_f_t.html#a5a2f8d9e47fc25902c365cd8b7c87aa2",
"group___transaction.html#a5518e05109ea6a098343f4e2a0f58711",
"http__server_8hpp.html",
"logger_8cpp.html",
"md_doc_coding_practices.html#autotoc_md189",
"namespacetaraxa.html#structtaraxa_1_1_magnolia_hardfork",
"state__api__data_8hpp.html#ga5c809a2ce1cbc35fa9a7d77abf02b1f7",
"structdev_1_1p2p_1_1_host.html#ab2c30507ea8316ba74331f5042a11bd6",
"structdev_1_1p2p_1_1_session.html#a9f81114e13214a9c93614252f2cf24e3",
"structtaraxa_1_1_vrf_params.html#a0fa7b78e010d7e0b3e19ff13e02bd1f2",
"types_8hpp.html#aee86984b7cf218e6347737f4f6cb5d60"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';