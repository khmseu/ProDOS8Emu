## Plan: MLI Dual-Address Operand Handling

Implement dual-address extraction for MLI-aligned calls so JSR target labels remain PC labels while the MLI parameter-block pointer is treated as a data-style operand label. This prevents false PRODOS8-at-param-block emissions and matches the intended semantics of MLI trace operands.

**Phases 3**

1. **Phase 1: Add Dual-Address Extraction API**
    - **Objective:** Enable operand extraction to return multiple label/address results for a matched source/trace pair.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py (`extract_operand_label_pair` path; add plural helper and keep compatibility wrapper)
    - **Tests to Write:** Self-check assertions for MLI dual-address extraction behavior.
    - **Steps:**
        1. Add failing self-check assertions for an MLI trace matched with source `JSR PRODOS8`.
        2. Implement plural extraction helper that supports MLI returning both PC and data semantics.
        3. Keep existing single-result helper as compatibility wrapper and ensure tests pass.

2. **Phase 2: Consume Multiple Operand Labels in Main Loop**
    - **Objective:** Allow one aligned instruction to contribute both PC and data label discoveries.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py (main alignment/discovery loop around operand label processing)
    - **Tests to Write:** Self-check assertion validating multi-label annotation and routing to discovered vs discovered_data.
    - **Steps:**
        1. Add failing self-check for multi-result consumption.
        2. Update loop to iterate extracted operand pairs and route each by flag.
        3. Re-run self-check and verify behavior.

3. **Phase 3: Regression Hardening for MLI Parameter Handling**
    - **Objective:** Ensure MLI parameter-block addresses are treated as data operands and not as JSR target aliases.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py (MLI extraction details + self-check regression assertions)
    - **Tests to Write:** Regression assertions for MLI `.byte/.word` parsing and symbolic-only data-label policy.
    - **Steps:**
        1. Add failing regression assertions for MLI parameter extraction and no synthetic labels.
        2. Refine extraction logic for MLI-specific path and symbolic-only policy.
        3. Run self-check and a targeted analyzer invocation to confirm expected output shape.

**Open Questions 1**

1. Resolved: parameter-block labels should be `symbolic-only` (emit only when source provides a label).
