/*
@licstart  The following is the entire license notice for the
JavaScript code in this file.

Copyright (C) 1997-2019 by Dimitri van Heesch

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as published by
the Free Software Foundation

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

@licend  The above is the entire license notice
for the JavaScript code in this file
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
      [ "Building on Ubuntu 20.04", "md_doc_building.html#autotoc_md27", [
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
      [ "Building on MacOS", "md_doc_building.html#autotoc_md31", [
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md32", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md33", null ],
        [ "Compile", "md_doc_building.html#autotoc_md34", null ],
        [ "Known issues", "md_doc_building.html#autotoc_md35", [
          [ "Issues with conan cache", "md_doc_building.html#autotoc_md36", null ],
          [ "Project building issue", "md_doc_building.html#autotoc_md37", null ]
        ] ]
      ] ],
      [ "Building on M1 macs", "md_doc_building.html#autotoc_md38", [
        [ "Install Rosetta2", "md_doc_building.html#autotoc_md39", null ],
        [ "Run an x86_64 session", "md_doc_building.html#autotoc_md40", null ],
        [ "Install Homebrew", "md_doc_building.html#autotoc_md41", null ],
        [ "Install dependencies", "md_doc_building.html#autotoc_md42", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md43", null ],
        [ "Compile", "md_doc_building.html#autotoc_md44", null ]
      ] ],
      [ "Run", "md_doc_building.html#autotoc_md45", [
        [ "Running tests", "md_doc_building.html#autotoc_md46", null ],
        [ "Running taraxa-node", "md_doc_building.html#autotoc_md47", null ]
      ] ],
      [ "Building using \"taraxa-builder\" docker image", "md_doc_building.html#autotoc_md48", null ]
    ] ],
    [ "C++ Best Practices Guidelines", "md_doc_coding_practices.html", [
      [ "The structure of this document", "md_doc_coding_practices.html#autotoc_md50", [
        [ "<a name=\"compiletimechecking\"></a> 1. Prefer compile-time checking to run-time checking", "md_doc_coding_practices.html#autotoc_md51", null ],
        [ "<a name=\"avoidnonconst\"></a> 2. Avoid non-<tt>const</tt> global variables", "md_doc_coding_practices.html#autotoc_md55", null ],
        [ "<a name=\"ruleofzero\"></a> 3. If you can avoid defining default operations, do", "md_doc_coding_practices.html#autotoc_md62", null ],
        [ "<a name=\"ruleoffive\"></a> 4. If you define or <tt>=delete</tt> any default operation, define or <tt>=delete</tt> them all", "md_doc_coding_practices.html#autotoc_md67", null ],
        [ "<a name=\"smartptr\"></a> 5. Never transfer ownership by a raw pointer (<tt>T*</tt>) or reference (<tt>T&</tt>)", "md_doc_coding_practices.html#autotoc_md76", null ],
        [ "<a name=\"usingsmartptr\"></a> 6. Choose appropriate smart pointer or why we have more smart pointers?", "md_doc_coding_practices.html#autotoc_md81", null ],
        [ "<a name=\"singlealloc\"></a> 7. Perform at most one explicit resource allocation in a single expression statement", "md_doc_coding_practices.html#autotoc_md85", null ],
        [ "<a name=\"passsmartptr\"></a> 8. Take smart pointers as parameters only to explicitly express lifetime semantics", "md_doc_coding_practices.html#autotoc_md89", null ],
        [ "<a name=\"varlimitscope\"></a> 9. Declare names in for-statement initializers and conditions to limit scope", "md_doc_coding_practices.html#autotoc_md94", null ],
        [ "<a name=\"varrecycle\"></a> 10. Don't use a variable for two unrelated purposes", "md_doc_coding_practices.html#autotoc_md100", null ],
        [ "<a name=\"macrosconsts\"></a> 11. Don't use macros for constants or \"functions\"", "md_doc_coding_practices.html#autotoc_md105", null ],
        [ "<a name=\"magicconstants\"></a> 12. Avoid \"magic constants\"; use symbolic constants", "md_doc_coding_practices.html#autotoc_md109", null ],
        [ "<a name=\"nullptr\"></a> 13. Use <tt>nullptr</tt> rather than <tt>0</tt> or <tt>NULL</tt>", "md_doc_coding_practices.html#autotoc_md113", null ],
        [ "<a name=\"construct\"></a> 14. Use the <tt>T{e}</tt>notation for construction", "md_doc_coding_practices.html#autotoc_md117", null ],
        [ "<a name=\"referencecapture\"></a> 15. Prefer capturing by reference in lambdas that will be used locally, including passed to algorithms", "md_doc_coding_practices.html#autotoc_md123", null ],
        [ "<a name=\"valuecapture\"></a> 16. Avoid capturing by reference in lambdas that will be used nonlocally, including returned, stored on the heap, or passed to another thread", "md_doc_coding_practices.html#autotoc_md130", null ],
        [ "<a name=\"thiscapture\"></a> 17. If you capture <tt>this</tt>, capture all variables explicitly (no default capture)", "md_doc_coding_practices.html#autotoc_md135", null ],
        [ "<a name=\"defaultctor\"></a> 18. Don't define a default constructor that only initializes data members; use in-class member initializers instead", "md_doc_coding_practices.html#autotoc_md140", null ],
        [ "<a name=\"explicitctor\"></a> 19. By default, declare single-argument constructors explicit", "md_doc_coding_practices.html#autotoc_md145", null ],
        [ "<a name=\"alwaysinitialize\"></a> 20. Always initialize an object", "md_doc_coding_practices.html#autotoc_md150", null ],
        [ "<a name=\"lambdainit\"></a> 21. Use lambdas for complex initialization, especially of <tt>const</tt> variables", "md_doc_coding_practices.html#autotoc_md161", null ],
        [ "<a name=\"orderctor\"></a> 22. Define and initialize member variables in the order of member declaration", "md_doc_coding_practices.html#autotoc_md167", null ],
        [ "<a name=\"inclassinitializer\"></a> 23. Prefer in-class initializers to member initializers in constructors for constant initializers", "md_doc_coding_practices.html#autotoc_md171", null ],
        [ "<a name=\"initializetoassignment\"></a> 24. Prefer initialization to assignment in constructors", "md_doc_coding_practices.html#autotoc_md176", null ],
        [ "<a name=\"castsnamed\"></a> 25. If you must use a cast, use a named cast", "md_doc_coding_practices.html#autotoc_md180", null ],
        [ "<a name=\"castsconst\"></a> 26. Don't cast away <tt>const</tt>", "md_doc_coding_practices.html#autotoc_md186", null ],
        [ "<a name=\"symmetric\"></a> 27. Use nonmember functions for symmetric operators", "md_doc_coding_practices.html#autotoc_md193", null ]
      ] ]
    ] ],
    [ "Contributing Guide", "md_doc_contributing.html", null ],
    [ "Doxygen", "md_doc_doxygen.html", [
      [ "Installing dependencies", "md_doc_doxygen.html#autotoc_md199", [
        [ "Ubuntu", "md_doc_doxygen.html#autotoc_md200", null ],
        [ "MacOS", "md_doc_doxygen.html#autotoc_md201", null ]
      ] ],
      [ "Generating documentation", "md_doc_doxygen.html#autotoc_md202", null ],
      [ "Commenting", "md_doc_doxygen.html#autotoc_md203", [
        [ "Basic example", "md_doc_doxygen.html#autotoc_md204", null ],
        [ "Grouping to modules", "md_doc_doxygen.html#autotoc_md205", null ],
        [ "Graphs", "md_doc_doxygen.html#autotoc_md206", null ]
      ] ]
    ] ],
    [ "Git-flow Guide", "md_doc_git_practices.html", [
      [ "Branch naming conventions", "md_doc_git_practices.html#autotoc_md207", null ],
      [ "Main branches", "md_doc_git_practices.html#autotoc_md208", null ],
      [ "Supporting branches", "md_doc_git_practices.html#autotoc_md209", [
        [ "Standard Feature branches", "md_doc_git_practices.html#autotoc_md210", null ],
        [ "Long-term Feature branches", "md_doc_git_practices.html#autotoc_md211", null ],
        [ "Hotfix branches", "md_doc_git_practices.html#autotoc_md212", null ],
        [ "Release branches", "md_doc_git_practices.html#autotoc_md213", null ]
      ] ],
      [ "Branches Cleaning", "md_doc_git_practices.html#autotoc_md214", null ],
      [ "PR merging & Code reviews", "md_doc_git_practices.html#autotoc_md215", null ],
      [ "Commit message conventions", "md_doc_git_practices.html#autotoc_md216", null ],
      [ "Automatic github issues linking", "md_doc_git_practices.html#autotoc_md220", null ],
      [ "Example", "md_doc_git_practices.html#autotoc_md221", null ]
    ] ],
    [ "Quickstart Guide", "md_doc_quickstart_guide.html", [
      [ "Taraxa docker image", "md_doc_quickstart_guide.html#autotoc_md230", [
        [ "Pre-requisites", "md_doc_quickstart_guide.html#autotoc_md223", [
          [ "\"type\" must be one of the following mentioned below!", "md_doc_git_practices.html#autotoc_md217", null ],
          [ "\"scope\" is optional", "md_doc_git_practices.html#autotoc_md218", null ],
          [ "\"subject\"", "md_doc_git_practices.html#autotoc_md219", null ],
          [ "Used git flow:", "md_doc_git_practices.html#autotoc_md222", null ],
          [ "MANDATORY PORT", "md_doc_quickstart_guide.html#autotoc_md224", null ],
          [ "OPTIONAL PORTS", "md_doc_quickstart_guide.html#autotoc_md225", null ]
        ] ],
        [ "Config", "md_doc_quickstart_guide.html#autotoc_md226", [
          [ "Param1", "md_doc_quickstart_guide.html#autotoc_md227", null ],
          [ "Param2", "md_doc_quickstart_guide.html#autotoc_md228", null ],
          [ "...", "md_doc_quickstart_guide.html#autotoc_md229", null ],
          [ "taraxa-builder:latest", "md_doc_quickstart_guide.html#autotoc_md231", null ],
          [ "taraxa-node:latest", "md_doc_quickstart_guide.html#autotoc_md232", null ]
        ] ]
      ] ]
    ] ],
    [ "Taraxa RPC", "md_doc__r_p_c.html", [
      [ "Ethereum compatibility", "md_doc__r_p_c.html#autotoc_md234", [
        [ "Quirks", "md_doc__r_p_c.html#autotoc_md235", null ],
        [ "Not implemented", "md_doc__r_p_c.html#autotoc_md236", null ]
      ] ],
      [ "Taraxa specific methods", "md_doc__r_p_c.html#autotoc_md237", [
        [ "taraxa_protocolVersion", "md_doc__r_p_c.html#autotoc_md238", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md239", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md240", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md241", null ]
        ] ],
        [ "taraxa_getVersion", "md_doc__r_p_c.html#autotoc_md242", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md243", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md244", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md245", null ]
        ] ],
        [ "taraxa_getDagBlockByHash", "md_doc__r_p_c.html#autotoc_md246", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md247", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md248", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md249", null ]
        ] ],
        [ "taraxa_getDagBlockByLevel", "md_doc__r_p_c.html#autotoc_md250", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md251", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md252", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md253", null ]
        ] ],
        [ "taraxa_dagBlockLevel", "md_doc__r_p_c.html#autotoc_md254", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md255", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md256", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md257", null ]
        ] ],
        [ "taraxa_dagBlockPeriod", "md_doc__r_p_c.html#autotoc_md258", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md259", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md260", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md261", null ]
        ] ],
        [ "taraxa_getScheduleBlockByPeriod", "md_doc__r_p_c.html#autotoc_md262", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md263", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md264", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md265", null ]
        ] ],
        [ "taraxa_getConfig", "md_doc__r_p_c.html#autotoc_md266", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md267", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md268", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md269", null ]
        ] ]
      ] ],
      [ "Test API", "md_doc__r_p_c.html#autotoc_md270", [
        [ "get_sortition_change", "md_doc__r_p_c.html#autotoc_md271", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md272", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md273", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md274", null ]
        ] ],
        [ "send_coin_transaction", "md_doc__r_p_c.html#autotoc_md275", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md276", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md277", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md278", null ]
        ] ],
        [ "get_account_address", "md_doc__r_p_c.html#autotoc_md279", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md280", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md281", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md282", null ]
        ] ],
        [ "get_peer_count", "md_doc__r_p_c.html#autotoc_md283", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md284", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md285", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md286", null ]
        ] ],
        [ "get_node_status", "md_doc__r_p_c.html#autotoc_md287", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md288", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md289", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md290", null ]
        ] ],
        [ "get_packets_stats", "md_doc__r_p_c.html#autotoc_md291", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md292", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md293", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md294", null ]
        ] ],
        [ "get_all_nodes", "md_doc__r_p_c.html#autotoc_md295", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md296", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md297", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md298", null ]
        ] ]
      ] ]
    ] ],
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
"class_server_interface.html#aab469c17069122d975dafa410856434d",
"classdev_1_1_r_l_p_stream.html#ad8b6ebd738c4eec7f4ded266489fe994",
"classdev_1_1p2p_1_1_node_table.html#ae9d84e561b21ee2ad0b7bbd2f61febf3",
"classtaraxa_1_1_db_storage.html#a0f71fda142f1ecfb890e7a4d044f73a4",
"classtaraxa_1_1_full_node.html#acd9c6b5ca7ab7b27e0f1f438bcbe98c9",
"classtaraxa_1_1net_1_1_eth_client.html#a7b5a12cb159a474b0f8de062720b0977",
"classtaraxa_1_1net_1_1rpc_1_1eth_1_1_eth_impl.html#a05051bdf84e5df1d7a4f6a9b920db275",
"classtaraxa_1_1network_1_1tarcap_1_1_packet_handler.html#a0b2d7afcbf210dbd66c7f23ba0d08e76",
"classtaraxa_1_1network_1_1tarcap_1_1_test_state.html#ab05981b6b3afd8e10a92f25c06107c18",
"dir_3b1a9d163f89b4c09af95dc61eb18843.html",
"group___d_a_g.html#a17c4083039acb260cf25ab7eaee459b6",
"group___final_chain.html#a14c08ea6c9fbfc910c086f34aa68ab4f",
"group___p_b_f_t.html#a4643fcb542532d3705fb1a61679204eb",
"group___transaction.html#classtaraxa_1_1_transaction_queue",
"libdevcore_2_exceptions_8h.html#a1c4cad491ccea98485acd91f7d80c264",
"md_doc_building.html#autotoc_md33",
"namespacemembers_func_o.html",
"rpc__error__handler_8hpp_source.html",
"structdev_1_1p2p_1_1_host.html#a7551c55d8bface84e5f697e558a4a694",
"structdev_1_1p2p_1_1_session.html#a01416d8957bb4ce1e009e37ba5c3f092",
"structtaraxa_1_1final__chain_1_1_final_chain_1_1_transaction_hashes.html#add6167a0b2b26619da96337a1c664fee",
"types_8hpp.html#ad0d27015d777d709bbfa9046a833ba55"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';