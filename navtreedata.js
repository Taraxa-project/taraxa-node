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
      [ "Useful doc", "index.html#autotoc_md11", null ],
      [ "System Requirements", "index.html#autotoc_md12", null ]
    ] ],
    [ "Building taraxa-node", "md_doc_building.html", [
      [ "Building on Ubuntu 22.04", "md_doc_building.html#autotoc_md27", [
        [ "Compile-Time Options (cmake)", "md_doc_building.html#autotoc_md25", [
          [ "Docker image", "index.html#autotoc_md4", null ],
          [ "Ubuntu binary", "index.html#autotoc_md5", null ],
          [ "Inside docker image", "index.html#autotoc_md8", null ],
          [ "Pre-built binary or manual build:", "index.html#autotoc_md9", null ],
          [ "CMAKE_BUILD_TYPE=[Release/Debug/RelWithDebInfo]", "md_doc_building.html#autotoc_md26", null ]
        ] ],
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md28", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md29", null ],
        [ "Compile", "md_doc_building.html#autotoc_md30", null ]
      ] ],
      [ "Building on Ubuntu 20.04", "md_doc_building.html#autotoc_md31", [
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md32", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md33", null ],
        [ "Compile", "md_doc_building.html#autotoc_md34", null ]
      ] ],
      [ "Building on MacOS", "md_doc_building.html#autotoc_md35", [
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md36", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md37", null ],
        [ "Compile", "md_doc_building.html#autotoc_md38", null ],
        [ "Known issues", "md_doc_building.html#autotoc_md39", [
          [ "Issues with conan cache", "md_doc_building.html#autotoc_md40", null ],
          [ "Project building issue", "md_doc_building.html#autotoc_md41", null ]
        ] ]
      ] ],
      [ "Building on M1 Macs for x86_64 with Rosetta2", "md_doc_building.html#autotoc_md42", [
        [ "Install Rosetta2", "md_doc_building.html#autotoc_md43", null ],
        [ "Run an x86_64 session", "md_doc_building.html#autotoc_md44", null ],
        [ "Install Homebrew", "md_doc_building.html#autotoc_md45", null ],
        [ "Install dependencies", "md_doc_building.html#autotoc_md46", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md47", null ],
        [ "Compile", "md_doc_building.html#autotoc_md48", null ]
      ] ],
      [ "Run", "md_doc_building.html#autotoc_md49", [
        [ "Running tests", "md_doc_building.html#autotoc_md50", null ],
        [ "Running taraxa-node", "md_doc_building.html#autotoc_md51", null ]
      ] ],
      [ "Building using \"taraxa-builder\" docker image", "md_doc_building.html#autotoc_md52", null ]
    ] ],
    [ "C++ Best Practices Guidelines", "md_doc_coding_practices.html", [
      [ "The structure of this document", "md_doc_coding_practices.html#autotoc_md54", [
        [ "<a name=\"compiletimechecking\"></a> 1. Prefer compile-time checking to run-time checking", "md_doc_coding_practices.html#autotoc_md55", null ],
        [ "<a name=\"avoidnonconst\"></a> 2. Avoid non-<tt>const</tt> global variables", "md_doc_coding_practices.html#autotoc_md59", null ],
        [ "<a name=\"ruleofzero\"></a> 3. If you can avoid defining default operations, do", "md_doc_coding_practices.html#autotoc_md66", null ],
        [ "<a name=\"ruleoffive\"></a> 4. If you define or <tt>=delete</tt> any default operation, define or <tt>=delete</tt> them all", "md_doc_coding_practices.html#autotoc_md71", null ],
        [ "<a name=\"smartptr\"></a> 5. Never transfer ownership by a raw pointer (<tt>T*</tt>) or reference (<tt>T&</tt>)", "md_doc_coding_practices.html#autotoc_md80", null ],
        [ "<a name=\"usingsmartptr\"></a> 6. Choose appropriate smart pointer or why we have more smart pointers?", "md_doc_coding_practices.html#autotoc_md85", null ],
        [ "<a name=\"singlealloc\"></a> 7. Perform at most one explicit resource allocation in a single expression statement", "md_doc_coding_practices.html#autotoc_md89", null ],
        [ "<a name=\"passsmartptr\"></a> 8. Take smart pointers as parameters only to explicitly express lifetime semantics", "md_doc_coding_practices.html#autotoc_md93", null ],
        [ "<a name=\"varlimitscope\"></a> 9. Declare names in for-statement initializers and conditions to limit scope", "md_doc_coding_practices.html#autotoc_md98", null ],
        [ "<a name=\"varrecycle\"></a> 10. Don't use a variable for two unrelated purposes", "md_doc_coding_practices.html#autotoc_md104", null ],
        [ "<a name=\"macrosconsts\"></a> 11. Don't use macros for constants or \"functions\"", "md_doc_coding_practices.html#autotoc_md109", null ],
        [ "<a name=\"magicconstants\"></a> 12. Avoid \"magic constants\"; use symbolic constants", "md_doc_coding_practices.html#autotoc_md113", null ],
        [ "<a name=\"nullptr\"></a> 13. Use <tt>nullptr</tt> rather than <tt>0</tt> or <tt>NULL</tt>", "md_doc_coding_practices.html#autotoc_md117", null ],
        [ "<a name=\"construct\"></a> 14. Use the <tt>T{e}</tt>notation for construction", "md_doc_coding_practices.html#autotoc_md121", null ],
        [ "<a name=\"referencecapture\"></a> 15. Prefer capturing by reference in lambdas that will be used locally, including passed to algorithms", "md_doc_coding_practices.html#autotoc_md127", null ],
        [ "<a name=\"valuecapture\"></a> 16. Avoid capturing by reference in lambdas that will be used nonlocally, including returned, stored on the heap, or passed to another thread", "md_doc_coding_practices.html#autotoc_md134", null ],
        [ "<a name=\"thiscapture\"></a> 17. If you capture <tt>this</tt>, capture all variables explicitly (no default capture)", "md_doc_coding_practices.html#autotoc_md139", null ],
        [ "<a name=\"defaultctor\"></a> 18. Don't define a default constructor that only initializes data members; use in-class member initializers instead", "md_doc_coding_practices.html#autotoc_md144", null ],
        [ "<a name=\"explicitctor\"></a> 19. By default, declare single-argument constructors explicit", "md_doc_coding_practices.html#autotoc_md149", null ],
        [ "<a name=\"alwaysinitialize\"></a> 20. Always initialize an object", "md_doc_coding_practices.html#autotoc_md154", null ],
        [ "<a name=\"lambdainit\"></a> 21. Use lambdas for complex initialization, especially of <tt>const</tt> variables", "md_doc_coding_practices.html#autotoc_md165", null ],
        [ "<a name=\"orderctor\"></a> 22. Define and initialize member variables in the order of member declaration", "md_doc_coding_practices.html#autotoc_md171", null ],
        [ "<a name=\"inclassinitializer\"></a> 23. Prefer in-class initializers to member initializers in constructors for constant initializers", "md_doc_coding_practices.html#autotoc_md175", null ],
        [ "<a name=\"initializetoassignment\"></a> 24. Prefer initialization to assignment in constructors", "md_doc_coding_practices.html#autotoc_md180", null ],
        [ "<a name=\"castsnamed\"></a> 25. If you must use a cast, use a named cast", "md_doc_coding_practices.html#autotoc_md184", null ],
        [ "<a name=\"castsconst\"></a> 26. Don't cast away <tt>const</tt>", "md_doc_coding_practices.html#autotoc_md190", null ],
        [ "<a name=\"symmetric\"></a> 27. Use nonmember functions for symmetric operators", "md_doc_coding_practices.html#autotoc_md197", null ]
      ] ]
    ] ],
    [ "Contributing Guide", "md_doc_contributing.html", null ],
    [ "Doxygen", "md_doc_doxygen.html", [
      [ "Installing dependencies", "md_doc_doxygen.html#autotoc_md203", [
        [ "Ubuntu", "md_doc_doxygen.html#autotoc_md204", null ],
        [ "MacOS", "md_doc_doxygen.html#autotoc_md205", null ]
      ] ],
      [ "Generating documentation", "md_doc_doxygen.html#autotoc_md206", null ],
      [ "Commenting", "md_doc_doxygen.html#autotoc_md207", [
        [ "Basic example", "md_doc_doxygen.html#autotoc_md208", null ],
        [ "Grouping to modules", "md_doc_doxygen.html#autotoc_md209", null ],
        [ "Graphs", "md_doc_doxygen.html#autotoc_md210", null ]
      ] ]
    ] ],
    [ "Git-flow Guide", "md_doc_git_practices.html", [
      [ "Branch naming conventions", "md_doc_git_practices.html#autotoc_md211", null ],
      [ "Main branches", "md_doc_git_practices.html#autotoc_md212", null ],
      [ "Supporting branches", "md_doc_git_practices.html#autotoc_md213", [
        [ "Standard Feature branches", "md_doc_git_practices.html#autotoc_md214", null ],
        [ "Long-term Feature branches", "md_doc_git_practices.html#autotoc_md215", null ],
        [ "Hotfix branches", "md_doc_git_practices.html#autotoc_md216", null ],
        [ "Release branches", "md_doc_git_practices.html#autotoc_md217", null ]
      ] ],
      [ "Branches Cleaning", "md_doc_git_practices.html#autotoc_md218", null ],
      [ "PR merging & Code reviews", "md_doc_git_practices.html#autotoc_md219", null ],
      [ "Commit message conventions", "md_doc_git_practices.html#autotoc_md220", null ],
      [ "Automatic github issues linking", "md_doc_git_practices.html#autotoc_md224", null ],
      [ "Example", "md_doc_git_practices.html#autotoc_md225", null ]
    ] ],
    [ "Quickstart Guide", "md_doc_quickstart_guide.html", [
      [ "Taraxa docker image", "md_doc_quickstart_guide.html#autotoc_md234", [
        [ "Pre-requisites", "md_doc_quickstart_guide.html#autotoc_md227", [
          [ "\"type\" must be one of the following mentioned below!", "md_doc_git_practices.html#autotoc_md221", null ],
          [ "\"scope\" is optional", "md_doc_git_practices.html#autotoc_md222", null ],
          [ "\"subject\"", "md_doc_git_practices.html#autotoc_md223", null ],
          [ "Used git flow:", "md_doc_git_practices.html#autotoc_md226", null ],
          [ "MANDATORY PORT", "md_doc_quickstart_guide.html#autotoc_md228", null ],
          [ "OPTIONAL PORTS", "md_doc_quickstart_guide.html#autotoc_md229", null ]
        ] ],
        [ "Config", "md_doc_quickstart_guide.html#autotoc_md230", [
          [ "Param1", "md_doc_quickstart_guide.html#autotoc_md231", null ],
          [ "Param2", "md_doc_quickstart_guide.html#autotoc_md232", null ],
          [ "...", "md_doc_quickstart_guide.html#autotoc_md233", null ],
          [ "taraxa-builder:latest", "md_doc_quickstart_guide.html#autotoc_md235", null ],
          [ "taraxa-node:latest", "md_doc_quickstart_guide.html#autotoc_md236", null ]
        ] ]
      ] ]
    ] ],
    [ "Standard release cycle", "md_doc_release_cycle.html", [
      [ "Release cycle phases", "md_doc_release_cycle.html#autotoc_md238", [
        [ "Phase 1 - active development of new features", "md_doc_release_cycle.html#autotoc_md239", null ],
        [ "Phase 2 - alpha testing (internal)", "md_doc_release_cycle.html#autotoc_md240", null ],
        [ "Phase 3 - beta testing (public)", "md_doc_release_cycle.html#autotoc_md241", null ],
        [ "Phase 4 - Mainnet release", "md_doc_release_cycle.html#autotoc_md242", null ]
      ] ],
      [ "Ad-hoc releases with bug fixes", "md_doc_release_cycle.html#autotoc_md243", null ]
    ] ],
    [ "Rewards distribution algorithm", "md_doc_rewards_distribution.html", [
      [ "Glossary", "md_doc_rewards_distribution.html#autotoc_md245", null ],
      [ "Rewards sources", "md_doc_rewards_distribution.html#autotoc_md246", null ],
      [ "Rewards distribution", "md_doc_rewards_distribution.html#autotoc_md247", [
        [ "Beneficial work in network", "md_doc_rewards_distribution.html#autotoc_md248", null ],
        [ "Newly created tokens:", "md_doc_rewards_distribution.html#autotoc_md249", null ],
        [ "Included transactions fees:", "md_doc_rewards_distribution.html#autotoc_md250", null ]
      ] ],
      [ "Validators statistics", "md_doc_rewards_distribution.html#autotoc_md251", null ],
      [ "Example:", "md_doc_rewards_distribution.html#autotoc_md252", [
        [ "DAG structure:", "md_doc_rewards_distribution.html#autotoc_md253", null ],
        [ "PBFT block", "md_doc_rewards_distribution.html#autotoc_md254", null ],
        [ "Statistics", "md_doc_rewards_distribution.html#autotoc_md255", null ],
        [ "Rewards", "md_doc_rewards_distribution.html#autotoc_md256", [
          [ "DAG blocks rewards", "md_doc_rewards_distribution.html#autotoc_md257", null ],
          [ "PBFT proposer reward", "md_doc_rewards_distribution.html#autotoc_md258", null ],
          [ "PBFT voters reward", "md_doc_rewards_distribution.html#autotoc_md259", null ]
        ] ]
      ] ]
    ] ],
    [ "Taraxa RPC", "md_doc__r_p_c.html", [
      [ "Ethereum compatibility", "md_doc__r_p_c.html#autotoc_md261", [
        [ "Quirks", "md_doc__r_p_c.html#autotoc_md262", null ],
        [ "Not implemented", "md_doc__r_p_c.html#autotoc_md263", null ]
      ] ],
      [ "Taraxa specific methods", "md_doc__r_p_c.html#autotoc_md264", [
        [ "taraxa_protocolVersion", "md_doc__r_p_c.html#autotoc_md265", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md266", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md267", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md268", null ]
        ] ],
        [ "taraxa_getVersion", "md_doc__r_p_c.html#autotoc_md269", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md270", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md271", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md272", null ]
        ] ],
        [ "taraxa_getDagBlockByHash", "md_doc__r_p_c.html#autotoc_md273", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md274", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md275", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md276", null ]
        ] ],
        [ "taraxa_getDagBlockByLevel", "md_doc__r_p_c.html#autotoc_md277", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md278", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md279", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md280", null ]
        ] ],
        [ "taraxa_dagBlockLevel", "md_doc__r_p_c.html#autotoc_md281", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md282", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md283", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md284", null ]
        ] ],
        [ "taraxa_dagBlockPeriod", "md_doc__r_p_c.html#autotoc_md285", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md286", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md287", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md288", null ]
        ] ],
        [ "taraxa_getScheduleBlockByPeriod", "md_doc__r_p_c.html#autotoc_md289", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md290", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md291", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md292", null ]
        ] ],
        [ "taraxa_pbftBlockHashByPeriod", "md_doc__r_p_c.html#autotoc_md293", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md294", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md295", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md296", null ]
        ] ],
        [ "taraxa_getConfig", "md_doc__r_p_c.html#autotoc_md297", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md298", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md299", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md300", null ]
        ] ]
      ] ],
      [ "Test API", "md_doc__r_p_c.html#autotoc_md301", [
        [ "get_sortition_change", "md_doc__r_p_c.html#autotoc_md302", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md303", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md304", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md305", null ]
        ] ],
        [ "send_coin_transaction", "md_doc__r_p_c.html#autotoc_md306", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md307", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md308", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md309", null ]
        ] ],
        [ "send_coin_transactions", "md_doc__r_p_c.html#autotoc_md310", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md311", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md312", null ]
        ] ],
        [ "get_account_address", "md_doc__r_p_c.html#autotoc_md313", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md314", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md315", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md316", null ]
        ] ],
        [ "get_peer_count", "md_doc__r_p_c.html#autotoc_md317", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md318", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md319", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md320", null ]
        ] ],
        [ "get_node_status", "md_doc__r_p_c.html#autotoc_md321", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md322", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md323", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md324", null ]
        ] ],
        [ "get_all_nodes", "md_doc__r_p_c.html#autotoc_md325", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md326", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md327", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md328", null ]
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
"_log_8h_source.html",
"class_server_interface.html#struct_server_interface_1_1_r_p_c_module",
"classdev_1_1_secure_fixed_hash.html#a55e7286a05eb1fc5072d118bff4eaae5",
"classdev_1_1p2p_1_1_peer.html#a345ef62a4ab8f2f27be1c99cec671fca",
"classgraphql_1_1taraxa_1_1_mutation.html#a0c3147e0992e5da52da3f4ae2bdf26b7",
"classtaraxa_1_1_db_storage.html#aedeb329641122c2397ea59163e42899d",
"classtaraxa_1_1cli_1_1_config.html#a57ec4dc90c87397725129f25e4a5934f",
"classtaraxa_1_1net_1_1_eth_face.html#afa0577e286f0a1c2f691161e79e6906e",
"classtaraxa_1_1net_1_1rpc_1_1eth_1_1_eth_impl.html#a4db54dcc9c6d41ea3a79c7daa54d5f48",
"classtaraxa_1_1network_1_1tarcap_1_1_packets_blocking_mask.html#a4489d756c7f43c016e4196982ec132ec",
"classtaraxa_1_1network_1_1tarcap_1_1_transaction_packet_handler.html#a0278fa38ffa38ca0e781671601501687",
"dir_08bf0f09b161111308ef9ead5ecd8aea.html",
"get__next__votes__sync__packet__handler_8cpp.html",
"group___final_chain.html#a3681f2fc87600ce90484dacfe07a14c6",
"group___p_b_f_t.html#a8d79d23ef300f120ac5a365f79092952",
"group___vote.html#aa0c587e8e6442373845d257b13fa7813",
"libp2p_2_common_8cpp.html",
"md_doc_coding_practices.html#autotoc_md145",
"namespacetaraxa.html#a13ec6be411c3c352858744678e0690da",
"state__config_8hpp.html#a79474e94e8b7bddd9319b13a2bbb18d0",
"structdev_1_1p2p_1_1_host.html#structdev_1_1p2p_1_1_host_1_1_persistent_state",
"structdev_1_1p2p_1_1_u_d_p_socket_face.html#a9106e4f9a26c0cda3b865c638a7788ed",
"structtaraxa_1_1util_1_1_default_construct_copyable_movable.html#a855a78293e92333fe5609f47714a79b5",
"verified__votes_8hpp.html#afc780cd89307f2097bb4281488e325bfab4f6c1353c76c2aecdfcd3fcc3e8241d"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';