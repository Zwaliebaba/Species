#!/usr/bin/env python3
"""Catch the call sites a math-type conversion leaves behind.

WHY THIS EXISTS. tasks/directxmath-migration.yaml replaces Vector3, Vector2,
Matrix33 and Matrix34 with DirectX::XMFLOAT3, XMFLOAT2, XMFLOAT3X3 and
XMFLOAT4X4. The legacy types carry methods and operators; the native ones carry
none. So converting a member or a signature silently invalidates every call
site that used those methods -- and none of those call sites mention the type
by name, which is why grepping for "Vector3" finds none of them.

That cost five red CI rounds in T10 alone, and T14-T20 convert hundreds of
members rather than nine. A CI round is two minutes; this is two seconds.

WHAT IT CHECKS. Given a member declared as a native type, any use of that
member with a legacy method or a vector operator is reported:

    m_pos.Mag()          XMFLOAT3 has no Mag
    m_centre - other     XMFLOAT3 has no operator-
    m_centre += other    nor any compound assignment (added by T16)
    m_centre == other    nor operator== -- and XMVector3Equal is NOT the same
                         test as the per-component NearlyEquals Vector3 had
                         (added by T15)
    m_transform.pos      XMFLOAT4X4 has no pos; the rows are _11.._44
    XMFLOAT3 m_vel;      Vector3() zeroed and XMFLOAT3() does not

and, independently of any member declaration, on LOCALS -- because converting a
local's type leaves every use of it behind, and those uses name no type:

    XMLoadFloat3(&mat.f) Matrix34's rows are Vector3, and Vector3 converts to
                         XMFLOAT3 by REFERENCE -- which does nothing for a
                         pointer (added by T16)
    XMLoadFloat3(&v)     the same, on a plain Vector3 local (added by T15)
    mat.ConvertToOpenGLFormat()
    mat.pos              where mat is an XMFLOAT4X4 (added by T17)

The last one is the important one: it is invisible to the compiler and to CI,
and it shipped twice during this migration before anything noticed. Shape's
CalculateCentre accumulated fragment centres onto uninitialised memory, and the
sound system's cached positions had the same defect with nothing to catch it.

WHAT IT DOES NOT CHECK. Transitive includes -- a header that stops supplying
Vector3 to files that were getting it through it. That one needs a compiler.
It is the one remaining failure mode on the list in T10's notes.

Exits 1 if anything is reported.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROJECTS = ["NeuronCore", "NeuronClient", "NeuronServer", "GameLogic", "Species", "Server", "Tests"]

NATIVE_VECTORS = ("XMFLOAT2", "XMFLOAT3")
NATIVE_MATRICES = ("XMFLOAT3X3", "XMFLOAT4X4")

# Methods the legacy types had and the native ones do not.
LEGACY_METHODS = (
    "Mag", "MagSquared", "Normalise", "Zero", "Set", "SetLength", "GetData", "GetDataConst",
    "HorizontalAndNormalise", "RotateAround", "RotateAroundX", "RotateAroundY", "RotateAroundZ",
    "FastRotateAround", "SetToIdentity", "Transpose", "Invert", "DecomposeToYDR", "IsNormalised",
    "ConvertToOpenGLFormat", "GetOr", "InverseMultiplyVector",
)

# Matrix34/Matrix33 named their rows; XMFLOAT4X4 numbers them.
LEGACY_MATRIX_FIELDS = ("pos", "r", "u", "f")

DECL = re.compile(
    r"^\s*(?:static\s+|mutable\s+|const\s+)*(?:DirectX::)?(XMFLOAT2|XMFLOAT3|XMFLOAT3X3|XMFLOAT4X4)\s+"
    r"(m_[A-Za-z0-9_]+)\s*(?P<init>[={;,])"
)

# ARRAY MEMBERS ARE DELIBERATELY NOT TRACKED, and T15 tried. Teaching DECL to
# match `XMFLOAT2 m_directions[6];` and every rule to see through a `[i]`
# subscript does catch a real bug -- Tripod's `m_directions[i] ^ delta` reached
# CI -- but it also reported three CORRECT lines, because `m_mousePos` is an
# int[3] in InputDriverWin32 and `m_right` is a float* in SoundLibrary3d. Those
# are T14's failure mode 6: a contended name that is not a math type at all,
# which this tool has no way to see. The header above says under-reporting is
# recoverable and crying wolf gets the tool switched off, so the trade was
# refused. Array members stay a hand-sweep item.

# The same member name can be native in one class and still legacy in another --
# m_pos is an XMFLOAT3 on SoundSource and a Vector3 on WorldObject. Resolving
# that needs a type checker, so a contended name is SKIPPED AND COUNTED rather
# than guessed at, which is the discipline check_containers.py settled on for
# exactly this reason. Under-reporting is recoverable; crying wolf on a
# thousand correct lines gets the tool switched off.
# Matrix34 locals, so the address-of rule below can tell a legacy row from a
# field that merely shares its name.
MATRIX34_LOCAL = re.compile(r"\bMatrix34\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*[=;]")

# Vector3/Vector2 locals, for the plain form of the same trap. A file that keeps
# a legacy local on purpose -- to feed a MathUtils out-parameter, say -- and then
# hands its address to XMLoadFloat3 has exactly the Matrix34-row bug without a
# matrix in sight. Tripod reached CI that way.
LEGACY_VECTOR_LOCAL = re.compile(r"\bVector([23])\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*[=;,]")

# XMFLOAT4X4 locals, for the mirror of that rule -- see below.
NATIVE_MATRIX_LOCAL = re.compile(r"\b(?:DirectX::)?XMFLOAT4X4\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*[=;]")

# XMMATRIX locals. Not checked -- they are here so the rules below can SKIP a
# name that means different things in different scopes of one file. XMMATRIX
# really does have an r[] array, and a file that converts `Matrix34 mat` to
# `XMFLOAT4X4 mat` in one function while loading an `XMMATRIX mat` in another
# is normal, not a bug.
XMMATRIX_LOCAL = re.compile(r"\bXMMATRIX\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*[=;]")

# XMFLOAT3/2 locals, and XMVECTOR locals to skip against. XMVECTOR DOES have
# arithmetic operators -- DirectXMath defines them -- so a name that is an
# XMFLOAT3 in one function and an XMVECTOR in another must not be accused.
NATIVE_VECTOR_LOCAL = re.compile(r"\b(?:DirectX::)?XMFLOAT[23]\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*[=;]")
XMVECTOR_LOCAL = re.compile(r"\bXMVECTOR\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*[=;]")

# An XMVECTOR carries no methods AT ALL -- it is four lanes in a register, not a
# class -- so any legacy method reached through one is unambiguously an error.
# Added by T15: `prevTailDir.HorizontalAndNormalise()` on a converted local
# reached CI twice in SporeGenerator.

# ANY member declaration whose type is not one of the math types. This is
# T14's failure mode 6 -- "a contended name that is not a math type at all" --
# and it is the reason two otherwise-good rules had to be refused before it was
# modelled: m_orders is an int on Officer and an XMFLOAT3 on Citizen,
# m_mousePos is an int[3] in InputDriverWin32, m_right is a float* in
# SoundLibrary3d. Collecting these lets the same contended-name skip that
# already protects Vector3-vs-XMFLOAT3 protect int-vs-XMFLOAT3 too.
#
# Deliberately greedy: over-collecting only causes a name to be SKIPPED, and
# the header above says under-reporting is the recoverable direction.
#
# But greedy is not the same as careless. `return m_avoidObstruction;` has the
# exact shape of a declaration -- identifier, space, m_name, terminator -- so
# without the keyword guard below every `return m_x;` in the tree silently
# marks m_x contended and switches off every rule for it. The guard is what
# keeps the skip list to genuinely ambiguous names.
STATEMENT_KEYWORDS = (
    "return|delete|case|throw|new|goto|else|do|co_return|co_await|co_yield|sizeof|typedef|using|friend|template"
)

NON_MATH_DECL = re.compile(
    r"^\s*(?:static\s+|mutable\s+|const\s+|inline\s+|virtual\s+)*"
    r"(?!(?:" + STATEMENT_KEYWORDS + r")\b)"
    r"(?!(?:DirectX::)?(?:XMFLOAT4X4|XMFLOAT3X3|XMFLOAT3|XMFLOAT2|XMVECTOR|XMMATRIX|Vector3|Vector2|Matrix34|Matrix33|Plane)\s)"
    r"[A-Za-z_][A-Za-z0-9_:]*\s*[*&]?\s+(m_[A-Za-z0-9_]+)\s*[\[={;,]"
)

LEGACY_DECL = re.compile(
    r"^\s*(?:static\s+|mutable\s+|const\s+)*(Vector2|Vector3|Matrix33|Matrix34)\s+"
    r"(m_[A-Za-z0-9_]+)\s*[={;,]"
)


def source_files():
    for project in PROJECTS:
        base = ROOT / project
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix in (".cpp", ".h", ".inc") and path.is_file():
                yield path


def strip_comment(line):
    """Good enough: this is a lint, and a false negative inside a comment is
    better than a false positive on one."""
    cut = line.find("//")
    return line if cut < 0 else line[:cut]


def blank_block_comments(text):
    """Blank out comments and string literals, keeping every newline so line
    numbers hold.

    Added by T17, and it has to be a real scanner rather than a find("/*").
    This tree is full of banner lines like

        //*****************************************

    and a naive search finds a "/*" one character in, then blanks everything
    up to the next "*/" -- which in practice means a whole file. That mistake
    cost two of T14's own verification scripts a wrong answer before it was
    understood, so it is written out properly here: line comments win over
    block comments, and string literals hide both.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                if text[i] == "\n":
                    out.append("\n")
                i += 1
            i += 2
        elif c in ('"', "'"):
            quote = c
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    i += 1
                i += 1
            i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


LOCAL_DECL = re.compile(
    r"^\s*(?:const\s+|static\s+)*(?:DirectX::)?"
    r"(?:XMFLOAT4X4|XMFLOAT3X3|XMFLOAT3|XMFLOAT2|XMVECTOR|XMMATRIX|Matrix34|Matrix33|Vector3|Vector2)\s+"
    r"(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*[=;({,]"
)


def stale_after_rename(rel, text):
    """A math local USED ABOVE THE LINE THAT DECLARES IT, which in this
    migration means a rename that left a use behind.

    T15 renamed Triffid::Render's `mat` to `headMatrix` so the pre-wobble and
    post-wobble matrices could be told apart, and missed the stem line -- which
    then bound to the NEW `mat` declared forty lines further down and reached
    CI as an undeclared identifier. Every other rule in this tool is about a
    type that changed; this one is about a name that moved, and it is the only
    failure mode here the compiler was catching alone.

    Two narrowings, both measured against the real bug rather than guessed:
    a line that itself declares the name is not a use of the later one, and a
    name declared TWICE in a body is two variables in two scopes, so it is
    skipped -- the same contended-name discipline as everywhere else. With
    both, the tree reports the Triffid bug and nothing else; with neither, four
    correct lines are accused.
    """
    problems = []
    lines = text.splitlines()
    depth = 0
    body = None
    for number, raw in enumerate(lines, 1):
        line = strip_comment(raw)
        opens, closes = line.count("{"), line.count("}")
        if depth == 0 and opens and body is None:
            body = []
        if body is not None:
            body.append((number, line))
        depth += opens - closes
        if body is None or depth != 0:
            continue

        declared = {}
        for at, l in body:
            found = LOCAL_DECL.match(l)
            if found:
                declared.setdefault(found.group(1), at)

        for name, at in declared.items():
            any_decl = re.compile(
                r"^\s*(?:[A-Za-z_][A-Za-z0-9_:<>]*\s*[*&]?\s+)+[A-Za-z0-9_,\s*&]*\b%s\b\s*[=;,)]" % re.escape(name))
            if sum(1 for _, l in body if any_decl.match(l)) > 1:
                continue
            use = re.compile(r"(?<![.>\w])%s\b(?!\s*[:.]\w)" % re.escape(name))
            for number_of_use, l in body:
                if number_of_use >= at:
                    break
                if use.search(l):
                    problems.append((rel, number_of_use,
                                     "%s is used here but not declared until line %d -- a rename that left a use "
                                     "behind, now binding to a different local" % (name, at), l.strip()))
                    break
        body = None
    return problems


def main():
    native_members = {}   # member name -> (kind, first declaring file)
    legacy_members = set()
    non_math_members = set()
    uninitialised = []

    for path in source_files():
        rel = path.relative_to(ROOT)
        for number, raw in enumerate(blank_block_comments(path.read_text(encoding="utf-8", errors="replace")).splitlines(), 1):
            line = strip_comment(raw)
            legacy = LEGACY_DECL.match(line)
            if legacy:
                legacy_members.add(legacy.group(2))

            non_math = NON_MATH_DECL.match(line)
            if non_math:
                non_math_members.add(non_math.group(1))

            found = DECL.match(line)
            if not found:
                continue
            kind, member = found.group(1), found.group(2)
            native_members.setdefault(member, ("matrix" if kind in NATIVE_MATRICES else "vector", rel))
            # A declaration that neither initialises nor is a function parameter.
            if kind in NATIVE_VECTORS and found.group("init") == ";":
                uninitialised.append((rel, number, member, kind, raw.strip()))

    ambiguous = sorted(set(native_members) & (legacy_members | non_math_members))
    for member in ambiguous:
        del native_members[member]
    uninitialised = [u for u in uninitialised if u[2] not in ambiguous]

    # RESCUING A CONTENDED NAME FOR THE ONE FILE THAT OWNS IT. The skip above
    # is tree-wide, and it is too blunt on its own: m_targetPos is XMFLOAT3 on
    # six converted classes and still Vector3 on Airstrike and Spam, so the
    # skip switched every rule off for it everywhere -- and two forgotten
    # `m_targetPos == g_zeroVector` in SoulDestroyer and Centipede reached CI
    # in T15 with the checker green.
    #
    # But X.cpp's paired X.h is not a guess about which class a bare m_x
    # belongs to: inside X.cpp, an unqualified member reference is X's, or a
    # base of X's, and either way the declaration in X.h is the one that binds.
    # So a contended name declared native in the file's OWN header is checked
    # in that file.
    #
    # Restricted to BARE uses -- the (?<![.>\w]) below. Without that, Citizen.h
    # declaring `XMFLOAT3 m_orders` made three correct `officer->m_orders`
    # lines in Citizen.cpp look wrong, since Officer's m_orders is an int.
    # Measured before it was kept: with the restriction, both real T15 bugs are
    # caught and nothing else is reported.
    rescued_by_file = {}
    for path in source_files():
        if path.suffix != ".cpp":
            continue
        header = path.with_suffix(".h")
        if not header.is_file():
            continue
        own = {}
        for raw in blank_block_comments(header.read_text(encoding="utf-8", errors="replace")).splitlines():
            found = DECL.match(strip_comment(raw))
            if found:
                own[found.group(2)] = "matrix" if found.group(1) in NATIVE_MATRICES else "vector"
        rescued = {member: kind for member, kind in own.items() if member in ambiguous}
        if rescued:
            rescued_by_file[path] = rescued

    problems = []
    for path in source_files():
        rel = path.relative_to(ROOT)
        text = blank_block_comments(path.read_text(encoding="utf-8", errors="replace"))

        # THE ADDRESS-OF TRAP, and the reason it needs its own rule. Vector3
        # converts to XMFLOAT3 through `operator XMFLOAT3 const&`, which makes
        # `XMFLOAT3 const a = someVector3;` compile and reads as though the two
        # types are interchangeable. They are not interchangeable through a
        # POINTER: `&someVector3` is a Vector3*, and XMLoadFloat3 wants an
        # XMFLOAT3*. Every Matrix34 row is a Vector3, so `XMLoadFloat3(&mat.f)`
        # on a matrix that came back from ShapeMarker::GetWorldMatrix -- still
        # Matrix34 until T10's seam closes -- fails to compile.
        #
        # Six of those reached CI in T16. Matched against Matrix34 locals
        # declared in the same file rather than any `.f`, so a struct that
        # happens to have a field called f is not accused.
        matrix34_locals = set(MATRIX34_LOCAL.findall(text))
        legacy_vector_locals = {name for _, name in LEGACY_VECTOR_LOCAL.findall(text)}

        # THE MIRROR OF THAT RULE, and the one that let Tree.cpp reach CI.
        # Converting `Matrix34 mat(...)` to an XMFLOAT4X4 leaves every USE of
        # that local behind, and those uses name no type: mat.ConvertToOpenGLFormat(),
        # mat.pos, mat.Invert(). The member rules above cannot see them because
        # a local is not a member. XMFLOAT4X4 has none of these, so any hit is
        # a real error rather than a guess.
        native_matrix_locals = set(NATIVE_MATRIX_LOCAL.findall(text))
        xmmatrix_locals = set(XMMATRIX_LOCAL.findall(text))

        # Same discipline as the contended member names above: a local name
        # that means two things in one file is skipped rather than guessed at.
        # A name that is a Vector3 in one scope and an XMFLOAT3 in another is
        # skipped, same discipline as everywhere else in this tool.
        legacy_vector_locals -= set(NATIVE_VECTOR_LOCAL.findall(text))
        legacy_vector_locals -= set(native_members) | set(XMVECTOR_LOCAL.findall(text))

        # An XMFLOAT4X4 local contended with an XMMATRIX one is not fully lost.
        # XMMATRIX has EXACTLY ONE of the four legacy row names -- r, its
        # XMVECTOR[4] -- so .pos, .u and .f stay unambiguously wrong on such a
        # name and only .r has to be given up. Triffid.cpp is why: its Render
        # wrote `mat.f *= ...`, `mat.u *= ...`, `mat.r *= ...` on an XMFLOAT4X4
        # while its GetHead loaded an `XMMATRIX mat`, and the blanket skip below
        # let all three reach CI in T15. Two of the three are recoverable.
        #
        # Contention with a MATRIX34 of the same name gives up everything --
        # Matrix34 has all four names -- so those are excluded here.
        half_contended_matrix_locals = (native_matrix_locals & xmmatrix_locals) - matrix34_locals

        contended_locals = (native_matrix_locals & matrix34_locals) | (native_matrix_locals & xmmatrix_locals)
        native_matrix_locals -= contended_locals
        matrix34_locals -= contended_locals

        # The same for vector locals. A converted local keeps every use it had,
        # and `(a - b).Normalise()` names no type either -- that one reached CI
        # in T17.
        vector_locals = set(NATIVE_VECTOR_LOCAL.findall(text))
        vector_locals -= set(XMVECTOR_LOCAL.findall(text))
        vector_locals -= set(re.findall(r"\bVector[23]\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*[=;]", text))
        vector_locals -= set(native_members) | legacy_members

        # XMVECTOR locals, minus any name that is something else elsewhere in
        # the file -- same contended-name discipline as everywhere here.
        xmvector_locals = set(XMVECTOR_LOCAL.findall(text))
        xmvector_locals -= set(NATIVE_VECTOR_LOCAL.findall(text)) | legacy_vector_locals
        xmvector_locals -= set(native_members) | legacy_members

        if path.suffix == ".cpp":
            problems.extend(stale_after_rename(rel, text))

        for number, raw in enumerate(text.splitlines(), 1):
            line = strip_comment(raw)

            for local in native_matrix_locals:
                if local not in line:
                    continue
                for method in LEGACY_METHODS:
                    if re.search(r"\b%s\s*\.\s*%s\s*\(" % (re.escape(local), method), line):
                        problems.append((rel, number, "%s.%s() -- %s is an XMFLOAT4X4 and has no %s"
                                         % (local, method, local, method), raw.strip()))
                for field in LEGACY_MATRIX_FIELDS:
                    if re.search(r"\b%s\s*\.\s*%s\b" % (re.escape(local), field), line):
                        problems.append((rel, number, "%s.%s -- %s is an XMFLOAT4X4, which numbers its rows _11 to _44"
                                         % (local, field, local), raw.strip()))

            for local in half_contended_matrix_locals:
                if local not in line:
                    continue
                for field in ("pos", "u", "f"):
                    if re.search(r"\b%s\s*\.\s*%s\b" % (re.escape(local), field), line):
                        problems.append((rel, number,
                                         "%s.%s -- %s is an XMFLOAT4X4 here, which numbers its rows _11 to _44, "
                                         "and an XMMATRIX elsewhere in this file, which has only .r"
                                         % (local, field, local), raw.strip()))

            for local in xmvector_locals:
                if local not in line:
                    continue
                for method in LEGACY_METHODS:
                    if re.search(r"(?<![.>\w])%s\s*\.\s*%s\s*\(" % (re.escape(local), method), line):
                        problems.append((rel, number, "%s.%s() -- %s is an XMVECTOR, which has no methods at all"
                                         % (local, method, local), raw.strip()))

            for local in vector_locals:
                if local not in line:
                    continue
                # (?<![.>\w]) so `something.pos.Mag()` is not read as a bare
                # local called pos -- Matrix34's .pos really does have Mag, and
                # EntityLeg reaches it through T10's seam on purpose.
                for method in LEGACY_METHODS:
                    if re.search(r"(?<![.>\w])%s\s*\.\s*%s\s*\(" % (re.escape(local), method), line):
                        problems.append((rel, number, "%s.%s() -- %s is an XMFLOAT3 and has no %s"
                                         % (local, method, local, method), raw.strip()))
                if re.search(r"(?<![.>\w])%s\s*(?:[-+*^]|/(?!/))\s*[A-Za-z_(]" % re.escape(local), line):
                    problems.append((rel, number, "arithmetic on %s -- it is an XMFLOAT3, which has no operators" % local,
                                     raw.strip()))
                if re.search(r"(?<![.>\w])%s\s*(?:[-+*^/]=)(?!=)" % re.escape(local), line):
                    problems.append((rel, number, "compound assignment to %s -- it is an XMFLOAT3, which has no operators" % local,
                                     raw.strip()))

            for local in legacy_vector_locals:
                if local not in line:
                    continue
                if re.search(r"XMLoadFloat[23]\s*\(\s*&\s*%s\b" % re.escape(local), line):
                    problems.append((rel, number,
                                     "XMLoadFloat3(&%s) -- %s is a Vector3, and the seam's conversion is to a reference, "
                                     "so it does not apply through a pointer" % (local, local),
                                     raw.strip()))

            for local in matrix34_locals:
                if local not in line:
                    continue
                if re.search(r"XMLoadFloat[23]\s*\(\s*&\s*%s\s*\.\s*(?:pos|r|u|f)\b" % re.escape(local), line):
                    problems.append((rel, number,
                                     "XMLoadFloat3(&%s.<row>) -- Matrix34's rows are Vector3, and the seam's "
                                     "conversion is to a reference, so it does not apply through a pointer" % local,
                                     raw.strip()))
            # Tree-wide members match anywhere, including through a `->`.
            # Rescued ones match only bare, for the reason given above.
            checked = [(member, kind, r"\b") for member, (kind, _) in native_members.items()]
            checked += [(member, kind, r"(?<![.>\w])") for member, kind in rescued_by_file.get(path, {}).items()]

            for member, kind, at in checked:
                if member not in line:
                    continue
                for method in LEGACY_METHODS:
                    if re.search(r"%s%s\s*\.\s*%s\s*\(" % (at, re.escape(member), method), line):
                        problems.append((rel, number, "%s.%s() -- the native type has no %s"
                                         % (member, method, method), raw.strip()))
                if kind == "matrix":
                    for field in LEGACY_MATRIX_FIELDS:
                        if re.search(r"%s%s\s*\.\s*%s\b" % (at, re.escape(member), field), line):
                            problems.append((rel, number, "%s.%s -- XMFLOAT4X4 numbers its rows, _11 to _44"
                                             % (member, field), raw.strip()))
                if re.search(r"%s%s\s*(?:[-+*^]|/(?!/))\s*[A-Za-z_(]" % (at, re.escape(member)), line):
                    problems.append((rel, number, "arithmetic on %s -- the native types have no operators" % member,
                                     raw.strip()))
                # Compound assignment was invisible until T16: the pattern above
                # requires an identifier after the operator, and `+=` has an `=`
                # there. These are the MUTATING operators, so they are the ones
                # whose absence matters most -- two of them reached CI.
                if re.search(r"%s%s\s*(?:[-+*^/]=)(?!=)" % (at, re.escape(member)), line):
                    problems.append((rel, number,
                                     "compound assignment to %s -- the native types have no operators" % member,
                                     raw.strip()))
                # Equality too. Vector3::operator== was a per-component
                # NearlyEquals at 1e-6; XMFLOAT3 has no operator== at all, and
                # two of these reached CI in T15 after being spotted and then
                # forgotten. XMVector3NearEqual with 1e-6 per lane is the
                # equivalent -- XMVector3Equal is a DIFFERENT test.
                if re.search(r"%s%s\s*[=!]=" % (at, re.escape(member)), line):
                    problems.append((rel, number,
                                     "%s compared with == or != -- XMFLOAT3 has neither; Vector3's was a "
                                     "per-component NearlyEquals at 1e-6, so XMVector3NearEqual is the match" % member,
                                     raw.strip()))

    for rel, number, message, text in problems:
        print("%s:%d: %s\n    %s" % (rel, number, message, text))

    if uninitialised:
        if problems:
            print()
        print("Native vector members with no initialiser. Vector3's default constructor")
        print("zeroed and XMFLOAT3's does not, so anything that accumulated into one, or")
        print("read it before first write, silently changed behaviour:")
        for rel, number, member, kind, text in uninitialised:
            print("  %s:%d: %s (%s)\n      %s" % (rel, number, member, kind, text))

    total = len(problems) + len(uninitialised)
    if total:
        print("\n%d site(s) need attention (%d member name(s) skipped as ambiguous)."
              % (total, len(ambiguous)))
        return 1

    print("Math types OK (%d native member name(s) tracked, %d ambiguous and skipped)."
          % (len(native_members), len(ambiguous)))
    if ambiguous:
        print("  skipped, the same name means something else somewhere: %s" % ", ".join(ambiguous))
    return 0


if __name__ == "__main__":
    sys.exit(main())
