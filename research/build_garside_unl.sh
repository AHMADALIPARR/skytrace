# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
# 1. Compile the zero-dependency C11 bracket-abstraction compiler
cc -O2 -std=c11 -Wall -Wextra garside_unl.c -o garside_unl

# 2. Emit the B_3 (3-strand) Garside normalizer in pure Unlambda
./garside_unl 3 > garside_b3.unl

# 3. Emit the B_4 (4-strand) Garside normalizer in pure Unlambda
./garside_unl 4 > garside_b4.unl

# 4. Run with any standard Unlambda interpreter
unlambda garside_b3.unl

