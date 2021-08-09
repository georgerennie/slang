//------------------------------------------------------------------------------
// NonConstFuncs.cpp
// Built-in non-constant system functions
//
// File is under the MIT license; see LICENSE for details
//------------------------------------------------------------------------------
#include "slang/binding/SystemSubroutine.h"
#include "slang/compilation/Compilation.h"
#include "slang/diagnostics/SysFuncsDiags.h"
#include "slang/symbols/ASTVisitor.h"
#include "slang/symbols/MemberSymbols.h"

namespace slang::Builtins {

class FErrorFunc : public SystemSubroutine {
public:
    FErrorFunc() : SystemSubroutine("$ferror", SubroutineKind::Function) { hasOutputArgs = true; }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, false, args, range, 2, 2))
            return comp.getErrorType();

        if (!args[0]->type->isIntegral())
            return badArg(context, *args[0]);

        if (!args[1]->verifyAssignable(context))
            return comp.getErrorType();

        const Type& ft = *args[1]->type;
        if (!ft.canBeStringLike()) {
            context.addDiag(diag::InvalidStringArg, args[1]->sourceRange) << ft;
            return comp.getErrorType();
        }

        return comp.getIntegerType();
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }
};

class FGetsFunc : public SystemSubroutine {
public:
    FGetsFunc() : SystemSubroutine("$fgets", SubroutineKind::Function) { hasOutputArgs = true; }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, false, args, range, 2, 2))
            return comp.getErrorType();

        if (!args[0]->verifyAssignable(context))
            return comp.getErrorType();

        const Type& ft = *args[0]->type;
        if (!ft.canBeStringLike()) {
            context.addDiag(diag::InvalidStringArg, args[0]->sourceRange) << ft;
            return comp.getErrorType();
        }

        if (!args[1]->type->isIntegral())
            return badArg(context, *args[1]);

        return comp.getIntegerType();
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }
};

class ScanfFunc : public SystemSubroutine {
public:
    explicit ScanfFunc(bool isFscanf) :
        SystemSubroutine(isFscanf ? "$fscanf" : "$sscanf", SubroutineKind::Function),
        isFscanf(isFscanf) {
        hasOutputArgs = true;
    }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, false, args, range, 2, INT32_MAX))
            return comp.getErrorType();

        // First argument is an fd or a string.
        if (isFscanf) {
            if (!args[0]->type->isIntegral())
                return badArg(context, *args[0]);
        }
        else {
            if (!args[0]->type->canBeStringLike()) {
                context.addDiag(diag::InvalidStringArg, args[0]->sourceRange) << *args[0]->type;
                return comp.getErrorType();
            }
        }

        // Second arg is a format string.
        if (!args[1]->type->canBeStringLike()) {
            context.addDiag(diag::InvalidStringArg, args[1]->sourceRange) << *args[1]->type;
            return comp.getErrorType();
        }

        // Rest of the args can be anything. It would be nice to add some compile-time
        // checking of the format string here in the future.

        return comp.getIntegerType();
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }

private:
    bool isFscanf;
};

class FReadFunc : public SystemSubroutine {
public:
    FReadFunc() : SystemSubroutine("$fread", SubroutineKind::Function) { hasOutputArgs = true; }

    bool allowEmptyArgument(size_t argIndex) const final { return argIndex == 2; }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, false, args, range, 2, 4))
            return comp.getErrorType();

        if (!args[0]->verifyAssignable(context))
            return comp.getErrorType();

        // First argument is integral or an unpacked array.
        if (!args[0]->type->isIntegral() && !args[0]->type->isUnpackedArray())
            return badArg(context, *args[0]);

        // Second argument is an fd (integral).
        if (!args[1]->type->isIntegral())
            return badArg(context, *args[1]);

        if (args.size() > 2) {
            // Third arg can be empty or integral.
            if (args[2]->kind != ExpressionKind::EmptyArgument && !args[2]->type->isIntegral())
                return badArg(context, *args[2]);

            if (args.size() > 3) {
                // Fourth arg must be integral.
                if (!args[3]->type->isIntegral())
                    return badArg(context, *args[3]);
            }
        }

        return comp.getIntegerType();
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }
};

class RandModeFunc : public SystemSubroutine {
public:
    RandModeFunc(const std::string& name) : SystemSubroutine(name, SubroutineKind::Task) {}

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        // The number of arguments required is 1 if called as a task
        // and 0 if called as a function.
        auto& comp = context.getCompilation();
        size_t numArgs = context.flags.has(BindFlags::TopLevelStatement) ? 1 : 0;
        if (!checkArgCount(context, true, args, range, numArgs, numArgs))
            return comp.getErrorType();

        if (numArgs) {
            // First argument is integral.
            if (!args[1]->type->isIntegral())
                return badArg(context, *args[1]);
        }

        return comp.getIntType();
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }
};

class DistributionFunc : public SystemSubroutine {
public:
    DistributionFunc(const std::string& name, size_t numArgs) :
        SystemSubroutine(name, SubroutineKind::Function), numArgs(numArgs) {
        hasOutputArgs = true;
    }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, false, args, range, numArgs, numArgs))
            return comp.getErrorType();

        for (size_t i = 0; i < numArgs; i++) {
            if (!args[i]->type->isIntegral())
                return badArg(context, *args[i]);
        }

        if (!args[0]->verifyAssignable(context))
            return comp.getErrorType();

        return comp.getIntType();
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }

private:
    size_t numArgs;
};

struct SampledValueExprVisitor {
    const BindContext& context;
    bool isFutureGlobal;

    SampledValueExprVisitor(const BindContext& context, bool isFutureGlobal) :
        context(context), isFutureGlobal(isFutureGlobal) {}

    template<typename T>
    void visit(const T& expr) {
        if constexpr (std::is_base_of_v<Expression, T>) {
            switch (expr.kind) {
                case ExpressionKind::NamedValue:
                    if (auto sym = expr.getSymbolReference()) {
                        if (sym->kind == SymbolKind::LocalAssertionVar ||
                            (sym->kind == SymbolKind::AssertionPort &&
                             sym->template as<AssertionPortSymbol>().isLocalVar())) {
                            context.addDiag(diag::SampledValueLocalVar, expr.sourceRange);
                        }
                    }
                    break;
                case ExpressionKind::Call: {
                    auto& call = expr.template as<CallExpression>();
                    if (call.isSystemCall()) {
                        if (call.getSubroutineName() == "matched"sv && !call.arguments().empty() &&
                            call.arguments()[0]->type->isSequenceType()) {
                            context.addDiag(diag::SampledValueMatched, expr.sourceRange);
                        }

                        if (isFutureGlobal && FutureGlobalNames.count(call.getSubroutineName())) {
                            context.addDiag(diag::GlobalSampledValueNested, expr.sourceRange);
                        }
                    }
                    break;
                }
                default:
                    if constexpr (is_detected_v<ASTDetectors::visitExprs_t, T,
                                                SampledValueExprVisitor>)
                        expr.visitExprs(*this);
                    break;
            }
        }
    }

    void visitInvalid(const Expression&) {}
    void visitInvalid(const AssertionExpr&) {}

    static inline const flat_hash_set<std::string_view> FutureGlobalNames = {
        "$future_gclk"sv, "$rising_gclk"sv, "$falling_gclk"sv, "$steady_gclk"sv, "$changing_gclk"sv
    };
};

static void checkSampledValueExpr(const Expression& expr, const BindContext& context,
                                  bool isFutureGlobal) {
    // Local variables and the sequence method "matched" are not allowed in the
    // expressions passed to sampled value functions.
    SampledValueExprVisitor visitor(context, isFutureGlobal);
    expr.visit(visitor);
}

class SampledFunc : public SystemSubroutine {
public:
    SampledFunc() : SystemSubroutine("$sampled", SubroutineKind::Function) {}

    const Expression& bindArgument(size_t, const BindContext& context,
                                   const ExpressionSyntax& syntax, const Args&) const final {
        return Expression::bind(syntax, context, BindFlags::AssertionExpr);
    }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, false, args, range, 1, 1))
            return comp.getErrorType();

        checkSampledValueExpr(*args[0], context, false);

        return *args[0]->type;
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }
};

class ValueChangeFunc : public SystemSubroutine {
public:
    ValueChangeFunc(const std::string& name) : SystemSubroutine(name, SubroutineKind::Function) {}

    const Expression& bindArgument(size_t, const BindContext& context,
                                   const ExpressionSyntax& syntax, const Args&) const final {
        return Expression::bind(syntax, context, BindFlags::AssertionExpr);
    }

    bool allowClockingArgument(size_t argIndex) const final { return argIndex == 1; }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, false, args, range, 1, 2))
            return comp.getErrorType();

        checkSampledValueExpr(*args[0], context, false);

        // TODO: check rules for inferring clocking

        if (args.size() == 2 && args[1]->kind != ExpressionKind::ClockingEvent)
            return badArg(context, *args[1]);

        return comp.getBitType();
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }
};

class PastFunc : public SystemSubroutine {
public:
    PastFunc() : SystemSubroutine("$past", SubroutineKind::Function) {}

    const Expression& bindArgument(size_t argIndex, const BindContext& context,
                                   const ExpressionSyntax& syntax, const Args&) const final {
        bitmask<BindFlags> extraFlags = BindFlags::None;
        if (argIndex == 0 || argIndex == 2)
            extraFlags = BindFlags::AssertionExpr;

        return Expression::bind(syntax, context, extraFlags);
    }

    bool allowEmptyArgument(size_t argIndex) const final { return argIndex == 1 || argIndex == 2; }
    bool allowClockingArgument(size_t argIndex) const final { return argIndex == 3; }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, false, args, range, 1, 4))
            return comp.getErrorType();

        for (size_t i = 0; i < args.size() && i < 3; i++)
            checkSampledValueExpr(*args[i], context, false);

        // TODO: check rules for inferring clocking

        if (args.size() > 1 && args[1]->kind != ExpressionKind::EmptyArgument) {
            auto numTicks = context.evalInteger(*args[1]);
            if (numTicks && *numTicks < 1)
                context.addDiag(diag::PastNumTicksInvalid, args[1]->sourceRange);
        }

        if (args.size() > 2 && args[2]->kind != ExpressionKind::EmptyArgument) {
            if (!context.requireBooleanConvertible(*args[2]))
                return comp.getErrorType();
        }

        if (args.size() > 3 && args[3]->kind != ExpressionKind::ClockingEvent)
            return badArg(context, *args[3]);

        return *args[0]->type;
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }
};

class GlobalValueChangeFunc : public SystemSubroutine {
public:
    GlobalValueChangeFunc(const std::string& name, bool isFuture) :
        SystemSubroutine(name, SubroutineKind::Function), isFuture(isFuture) {}

    const Expression& bindArgument(size_t, const BindContext& context,
                                   const ExpressionSyntax& syntax, const Args&) const final {
        return Expression::bind(syntax, context, BindFlags::AssertionExpr);
    }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, false, args, range, 1, 1))
            return comp.getErrorType();

        if (!comp.getGlobalClocking(*context.scope)) {
            context.addDiag(diag::NoGlobalClocking, range);
            return comp.getErrorType();
        }

        if (!context.flags.has(BindFlags::AssertionExpr) && isFuture) {
            context.addDiag(diag::GlobalSampledValueAssertionExpr, range);
            return comp.getErrorType();
        }

        checkSampledValueExpr(*args[0], context, isFuture);

        // TODO: enforce rules for future sampled value functions

        return comp.getBitType();
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }

private:
    bool isFuture;
};

void registerNonConstFuncs(Compilation& c) {
#define REGISTER(...) c.addSystemSubroutine(std::make_unique<NonConstantFunction>(__VA_ARGS__))

    auto& intType = c.getIntType();
    auto& uintType = c.getUnsignedIntType();
    std::vector<const Type*> intArg = { &intType };

    REGISTER("$time", c.getType(SyntaxKind::TimeType));
    REGISTER("$stime", c.getUnsignedIntType());
    REGISTER("$realtime", c.getType(SyntaxKind::RealTimeType));
    REGISTER("$random", intType, 0, intArg);
    REGISTER("$urandom", uintType, 0, intArg);
    REGISTER("$urandom_range", uintType, 1, std::vector<const Type*>{ &uintType, &uintType });

    REGISTER("$fopen", intType, 1, std::vector{ &c.getStringType(), &c.getStringType() });
    REGISTER("$fclose", c.getVoidType(), 1, intArg);
    REGISTER("$fgetc", intType, 1, intArg);
    REGISTER("$ungetc", intType, 2, std::vector{ &intType, &intType });
    REGISTER("$ftell", intType, 1, intArg);
    REGISTER("$fseek", intType, 3, std::vector{ &intType, &intType, &intType });
    REGISTER("$rewind", intType, 1, intArg);
    REGISTER("$fflush", c.getVoidType(), 0, intArg);
    REGISTER("$feof", intType, 1, intArg);

    REGISTER("$test$plusargs", intType, 1, std::vector{ &c.getStringType() });

#undef REGISTER

#define FUNC(name, numArgs) c.addSystemSubroutine(std::make_unique<DistributionFunc>(name, numArgs))
    FUNC("$dist_uniform", 3);
    FUNC("$dist_normal", 3);
    FUNC("$dist_exponential", 2);
    FUNC("$dist_poisson", 2);
    FUNC("$dist_chi_square", 2);
    FUNC("$dist_t", 2);
    FUNC("$dist_erlang", 3);
#undef FUNC

#define FUNC(name) c.addSystemSubroutine(std::make_unique<ValueChangeFunc>(name))
    FUNC("$rose");
    FUNC("$fell");
    FUNC("$stable");
    FUNC("$changed");
#undef FUNC

#define FUNC(name, isFuture) \
    c.addSystemSubroutine(std::make_unique<GlobalValueChangeFunc>(name, isFuture))
    FUNC("$past_gclk", false);
    FUNC("$rose_gclk", false);
    FUNC("$fell_gclk", false);
    FUNC("$stable_gclk", false);
    FUNC("$changed_gclk", false);
    FUNC("$future_gclk", true);
    FUNC("$rising_gclk", true);
    FUNC("$falling_gclk", true);
    FUNC("$steady_gclk", true);
    FUNC("$changing_gclk", true);
#undef FUNC

    c.addSystemSubroutine(std::make_unique<FErrorFunc>());
    c.addSystemSubroutine(std::make_unique<FGetsFunc>());
    c.addSystemSubroutine(std::make_unique<ScanfFunc>(true));
    c.addSystemSubroutine(std::make_unique<ScanfFunc>(false));
    c.addSystemSubroutine(std::make_unique<FReadFunc>());
    c.addSystemSubroutine(std::make_unique<SampledFunc>());
    c.addSystemSubroutine(std::make_unique<PastFunc>());

    c.addSystemMethod(SymbolKind::EventType,
                      std::make_unique<NonConstantFunction>("triggered", c.getBitType(), 0,
                                                            std::vector<const Type*>{},
                                                            /* isMethod */ true));

    c.addSystemMethod(SymbolKind::ClassProperty, std::make_unique<RandModeFunc>("rand_mode"));
    c.addSystemMethod(SymbolKind::ConstraintBlock,
                      std::make_unique<RandModeFunc>("constraint_mode"));
}

} // namespace slang::Builtins
