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
    m_transform.pos      XMFLOAT4X4 has no pos; the rows are _11.._44
    XMFLOAT3 m_vel;      Vector3() zeroed and XMFLOAT3() does not

and, independently of any member declaration:

    XMLoadFloat3(&mat.f) Matrix34's rows are Vector3, and Vector3 converts to
                         XMFLOAT3 by REFERENCE -- which does nothing for a
                         pointer (added by T16)

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

# The same member name can be native in one class and still legacy in another --
# m_pos is an XMFLOAT3 on SoundSource and a Vector3 on WorldObject. Resolving
# that needs a type checker, so a contended name is SKIPPED AND COUNTED rather
# than guessed at, which is the discipline check_containers.py settled on for
# exactly this reason. Under-reporting is recoverable; crying wolf on a
# thousand correct lines gets the tool switched off.
# Matrix34 locals, so the address-of rule below can tell a legacy row from a
# field that merely shares its name.
MATRIX34_LOCAL = re.compile(r"\bMatrix34\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*[=;]")

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


def main():
    native_members = {}   # member name -> (kind, first declaring file)
    legacy_members = set()
    uninitialised = []

    for path in source_files():
        rel = path.relative_to(ROOT)
        for number, raw in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            line = strip_comment(raw)
            legacy = LEGACY_DECL.match(line)
            if legacy:
                legacy_members.add(legacy.group(2))

            found = DECL.match(line)
            if not found:
                continue
            kind, member = found.group(1), found.group(2)
            native_members.setdefault(member, ("matrix" if kind in NATIVE_MATRICES else "vector", rel))
            # A declaration that neither initialises nor is a function parameter.
            if kind in NATIVE_VECTORS and found.group("init") == ";":
                uninitialised.append((rel, number, member, kind, raw.strip()))

    ambiguous = sorted(set(native_members) & legacy_members)
    for member in ambiguous:
        del native_members[member]
    uninitialised = [u for u in uninitialised if u[2] not in ambiguous]

    problems = []
    for path in source_files():
        rel = path.relative_to(ROOT)
        text = path.read_text(encoding="utf-8", errors="replace")

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

        for number, raw in enumerate(text.splitlines(), 1):
            line = strip_comment(raw)

            for local in matrix34_locals:
                if re.search(r"XMLoadFloat[23]\s*\(\s*&\s*%s\s*\.\s*(?:pos|r|u|f)\b" % re.escape(local), line):
                    problems.append((rel, number,
                                     "XMLoadFloat3(&%s.<row>) -- Matrix34's rows are Vector3, and the seam's "
                                     "conversion is to a reference, so it does not apply through a pointer" % local,
                                     raw.strip()))
            for member, (kind, _) in native_members.items():
                if member not in line:
                    continue
                for method in LEGACY_METHODS:
                    if re.search(r"\b%s\s*\.\s*%s\s*\(" % (re.escape(member), method), line):
                        problems.append((rel, number, "%s.%s() -- the native type has no %s"
                                         % (member, method, method), raw.strip()))
                if kind == "matrix":
                    for field in LEGACY_MATRIX_FIELDS:
                        if re.search(r"\b%s\s*\.\s*%s\b" % (re.escape(member), field), line):
                            problems.append((rel, number, "%s.%s -- XMFLOAT4X4 numbers its rows, _11 to _44"
                                             % (member, field), raw.strip()))
                if re.search(r"\b%s\s*(?:[-+*^]|/(?!/))\s*[A-Za-z_(]" % re.escape(member), line):
                    problems.append((rel, number, "arithmetic on %s -- the native types have no operators" % member,
                                     raw.strip()))
                # Compound assignment was invisible until T16: the pattern above
                # requires an identifier after the operator, and `+=` has an `=`
                # there. These are the MUTATING operators, so they are the ones
                # whose absence matters most -- two of them reached CI.
                if re.search(r"\b%s\s*(?:[-+*^/]=)(?!=)" % re.escape(member), line):
                    problems.append((rel, number,
                                     "compound assignment to %s -- the native types have no operators" % member,
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
        print("  skipped, declared as both a native and a legacy type: %s" % ", ".join(ambiguous))
    return 0


if __name__ == "__main__":
    sys.exit(main())
