#!/usr/bin/env python3
# kate: replace-tabs on; indent-width 4;

"""
Nanopb Validation Support Module
=================================

This module extends the nanopb generator to support declarative validation
constraints via custom options defined in validate.proto. It generates
validation code (C header and source files) for protobuf messages.

Architecture Overview
---------------------
The module consists of several key components:

1. **ValidationRule**: A dataclass representing a single validation constraint
   (e.g., "field must be >= 10", "string must match email format").

2. **FieldValidator**: Parses and stores validation rules for a single field.
   It extracts rules from the FieldRules protobuf option and converts them
   into ValidationRule objects.

3. **MessageValidator**: Aggregates field validators for all fields in a message,
   including handling of oneof groups. Also manages message-level validation rules.

4. **ValidatorGenerator**: The main code generator that produces C header and
   source files containing validation functions. It uses the validators to
   emit appropriate C code for each constraint.

Main Flow
---------
1. Input: ProtoFile with messages containing validate.proto options
2. Parse validation rules from each field and message
3. Generate validation header (.h) with function declarations and documentation
4. Generate validation source (.c) with implementation of validation functions
5. Output: Validation code that can be compiled alongside generated pb.c/pb.h files

Rule Types
----------
- Numeric constraints: GT, GTE, LT, LTE, EQ, IN, NOT_IN
- String constraints: MIN_LEN, MAX_LEN, PREFIX, SUFFIX, CONTAINS, ASCII
- Format validators: EMAIL, HOSTNAME, IP, IPV4, IPV6
- Repeated field constraints: MIN_ITEMS, MAX_ITEMS, UNIQUE, ITEMS
- Enum constraints: DEFINED_ONLY, IN, NOT_IN
- Message-level: REQUIRED, ONEOF_REQUIRED, and custom message rules
- Special: ANY_IN, ANY_NOT_IN for google.protobuf.Any fields

Code Generation Strategy
------------------------
The generator produces C code that uses macros from pb_validate.h to:
- Navigate field paths for clear error messages
- Accumulate violations, exiting early on the first one
- Recursively validate nested message fields
- Handle optional fields and pointers appropriately
"""

from collections import OrderedDict
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

# =============================================================================
# EXTERNAL DEPENDENCIES (from nanopb_generator.py)
# =============================================================================
# This module depends on exactly two items from nanopb_generator:
#
# 1. Globals.naming_style - Used for C identifier formatting:
#    - type_name(name) - Format a message name as C type (e.g., "Foo_Bar")
#    - var_name(name) - Format a field name as C variable (e.g., "field_name")
#
# 2. OneOf - The OneOf class type, used only for isinstance() checks
#
# These dependencies are isolated here and have fallback stubs for standalone use.
# If additional dependencies are needed, they MUST be added to this block.

try:
    from . import nanopb_generator
    Globals = nanopb_generator.Globals
    OneOf = nanopb_generator.OneOf
except (ImportError, AttributeError):
    # Fallback stubs for when used as standalone module
    class Globals:
        """Stub for Globals when nanopb_generator is not available."""
        class naming_style:
            @staticmethod
            def type_name(name):
                return str(name).replace('.', '_')
            @staticmethod
            def var_name(name):
                return str(name)
    class OneOf:
        """Stub for OneOf class when nanopb_generator is not available."""
        pass

# =============================================================================
# MODULE CONSTANTS
# =============================================================================
# Rule identifiers used throughout code generation.
# These constants provide type safety and make the code self-documenting.

RULE_REQUIRED = 'REQUIRED'
RULE_ONEOF_REQUIRED = 'ONEOF_REQUIRED'
RULE_EQ = 'EQ'
RULE_GT = 'GT'
RULE_GTE = 'GTE'
RULE_LT = 'LT'
RULE_LTE = 'LTE'
RULE_IN = 'IN'
RULE_NOT_IN = 'NOT_IN'
RULE_MIN_LEN = 'MIN_LEN'
RULE_MAX_LEN = 'MAX_LEN'
RULE_PREFIX = 'PREFIX'
RULE_SUFFIX = 'SUFFIX'
RULE_CONTAINS = 'CONTAINS'
RULE_ASCII = 'ASCII'
RULE_EMAIL = 'EMAIL'
RULE_HOSTNAME = 'HOSTNAME'
RULE_IP = 'IP'
RULE_IPV4 = 'IPV4'
RULE_IPV6 = 'IPV6'
RULE_ENUM_DEFINED = 'ENUM_DEFINED'
RULE_MIN_ITEMS = 'MIN_ITEMS'
RULE_MAX_ITEMS = 'MAX_ITEMS'
RULE_UNIQUE = 'UNIQUE'
RULE_ITEMS = 'ITEMS'
RULE_NO_SPARSE = 'NO_SPARSE'
RULE_ANY_IN = 'ANY_IN'
RULE_ANY_NOT_IN = 'ANY_NOT_IN'
RULE_TIMESTAMP_GT_NOW = 'TIMESTAMP_GT_NOW'
RULE_TIMESTAMP_LT_NOW = 'TIMESTAMP_LT_NOW'
RULE_TIMESTAMP_WITHIN = 'TIMESTAMP_WITHIN'


# =============================================================================
# HELPER FUNCTIONS
# =============================================================================

def _get_numeric_validator_info(type_name: str) -> Tuple[Optional[str], Optional[str]]:
    """
    Map a protobuf numeric type name to its C type and validator function.
    
    This centralizes the type mapping logic used throughout code generation.
    
    Args:
        type_name: The protobuf type name (e.g., 'int32', 'float', 'uint64')
        
    Returns:
        A tuple of (c_type, validator_function_name), or (None, None) if not found.
        
    Examples:
        >>> _get_numeric_validator_info('int32')
        ('int32_t', 'pb_validate_int32')
        >>> _get_numeric_validator_info('float')
        ('float', 'pb_validate_float')
    """
    TYPE_MAP = {
        'int32': ('int32_t', 'pb_validate_int32'),
        'sint32': ('int32_t', 'pb_validate_int32'),
        'sfixed32': ('int32_t', 'pb_validate_int32'),
        'int64': ('int64_t', 'pb_validate_int64'),
        'sint64': ('int64_t', 'pb_validate_int64'),
        'sfixed64': ('int64_t', 'pb_validate_int64'),
        'uint32': ('uint32_t', 'pb_validate_uint32'),
        'fixed32': ('uint32_t', 'pb_validate_uint32'),
        'uint64': ('uint64_t', 'pb_validate_uint64'),
        'fixed64': ('uint64_t', 'pb_validate_uint64'),
        'float': ('float', 'pb_validate_float'),
        'double': ('double', 'pb_validate_double'),
        'bool': ('bool', 'pb_validate_bool'),
        'enum': ('int', 'pb_validate_enum'),
    }
    return TYPE_MAP.get(type_name, (None, None))


def _escape_for_comment(s: Any) -> str:
    """
    Escape a string to be safe for use inside C comments and Doxygen blocks.
    
    Args:
        s: The string to escape (can be any type, will be converted to str)
        
    Returns:
        An escaped string safe for use in C comments.
        
    Note:
        - Prevents accidental comment closing by escaping */
        - Normalizes line endings
        - Handles None gracefully
    """
    try:
        if s is None:
            return ''
        # Avoid closing comment accidentally and normalize newlines
        return str(s).replace('*/', '* /').replace('\r\n', '\n').replace('\r', '\n')
    except Exception:
        return str(s) if s is not None else ''


def _escape_c_string(s: Any) -> str:
    """
    Escape a string to be safe for use in C string literals.
    
    Args:
        s: The string to escape (can be any type, will be converted to str)
        
    Returns:
        An escaped string safe for use in C string literals.
        
    Note:
        Backslashes are escaped first to avoid double-escaping.
    """
    try:
        if s is None:
            return ''
        # Escape backslashes first, then special characters
        s = str(s)
        s = s.replace('\\', '\\\\')
        s = s.replace('"', '\\"')
        s = s.replace('\n', '\\n')
        s = s.replace('\r', '\\r')
        s = s.replace('\t', '\\t')
        return s
    except Exception:
        return str(s) if s is not None else ''


# =============================================================================
# DATA STRUCTURES
# =============================================================================

@dataclass(frozen=True)
class CTypeInfo:
    """
    Encapsulates C type metadata for numeric field validation.
    
    This dataclass centralizes the C type information needed for code generation,
    making the relationship between protobuf types and C types explicit.
    
    Attributes:
        c_type: The C type name (e.g., 'int32_t', 'float')
        validator_func: The C validator function name (e.g., 'pb_validate_int32')
        proto_type: The original protobuf type name (e.g., 'int32', 'sint32')
    """
    c_type: str
    validator_func: str
    proto_type: str
    
    @classmethod
    def from_proto_type(cls, proto_type: str) -> Optional['CTypeInfo']:
        """Create CTypeInfo from a protobuf type name, or None if not a known type."""
        c_type, validator_func = _get_numeric_validator_info(proto_type)
        if c_type and validator_func:
            return cls(c_type=c_type, validator_func=validator_func, proto_type=proto_type)
        return None


@dataclass(frozen=True)
class ValidationRule:
    """
    Represents a single validation constraint on a field.
    
    A ValidationRule encapsulates one specific constraint to be enforced,
    such as "value must be greater than 10" or "string must be a valid email".
    
    Attributes:
        rule_type: The type of rule (e.g., RULE_GT, RULE_EMAIL, RULE_MIN_LEN).
                   Should be one of the RULE_* constants defined in this module.
        constraint_id: A unique identifier for this constraint, typically
                       in the format "type.rule" (e.g., "int32.gt", "string.email").
                       Used in error messages and documentation.
        params: Optional dictionary of parameters for the rule.
                For example, a GT rule might have {'value': 10},
                an IN rule might have {'values': [1, 2, 3]}.
                
    Examples:
        >>> ValidationRule(RULE_GT, 'int32.gt', {'value': 10})
        ValidationRule(rule_type='GT', constraint_id='int32.gt', params={'value': 10})
        
        >>> ValidationRule(RULE_EMAIL, 'string.email')
        ValidationRule(rule_type='EMAIL', constraint_id='string.email', params={})
    """
    rule_type: str
    constraint_id: str
    params: Dict[str, Any] = field(default_factory=dict)


@dataclass
class FieldContext:
    """
    Context for code generation that unifies regular fields and oneof members.
    
    This dataclass encapsulates all information needed to generate validation
    code for a field, whether it's a regular message field or a member of a
    oneof group. The unified context enables a single code generation path
    through the IR pipeline for all field types.
    
    Attributes:
        field: The protobuf field descriptor
        field_name: The field's name (used in generated code)
        is_oneof: True if this field is inside a oneof group
        oneof_name: Name of the containing oneof (None for regular fields)
        is_anonymous: True for C11 anonymous unions (field accessed directly)
        
    Properties:
        field_access: Returns the C expression to access this field from msg
                      E.g., "field_name" for regular/anonymous oneof,
                      "oneof_name.field_name" for non-anonymous oneof
        is_optional: Returns True if this is an optional field
    """
    field: Any
    field_name: str
    is_oneof: bool = False
    oneof_name: Optional[str] = None
    is_anonymous: bool = False
    
    @property
    def field_access(self) -> str:
        """Return C expression for accessing this field from msg pointer."""
        if self.is_oneof and not self.is_anonymous:
            return f"{self.oneof_name}.{self.field_name}"
        return self.field_name
    
    @property
    def is_optional(self) -> bool:
        """
        Return True if this field is optional (proto3 optional or proto2 optional).
        
        In nanopb, optional fields are identified by having `rules == 'OPTIONAL'` 
        in the field descriptor. These fields generate a `has_<field>` flag in the
        C struct that must be checked before validating the field value.
        """
        if self.field is None:
            return False
        try:
            return getattr(self.field, 'rules', None) == 'OPTIONAL'
        except Exception:
            return False
    
    @classmethod
    def for_regular_field(cls, field: Any) -> 'FieldContext':
        """Create context for a regular (non-oneof) field."""
        return cls(
            field=field,
            field_name=field.name,
            is_oneof=False,
            oneof_name=None,
            is_anonymous=False
        )
    
    @classmethod
    def for_oneof_member(cls, field: Any, oneof_name: str, anonymous: bool) -> 'FieldContext':
        """Create context for a field within a oneof group."""
        return cls(
            field=field,
            field_name=field.name,
            is_oneof=True,
            oneof_name=oneof_name,
            is_anonymous=anonymous
        )
    
    @classmethod
    def for_message_rule(cls) -> 'FieldContext':
        """
        Create a context for message-level rules.
        
        Message-level rules (REQUIRES, MUTEX, AT_LEAST) don't apply to a specific
        field, so this returns a context with no field information. This makes
        the intent explicit rather than using a raw FieldContext with None values.
        """
        return cls(
            field=None,
            field_name='',
            is_oneof=False,
            oneof_name=None,
            is_anonymous=False
        )


@dataclass
class RuleIR:
    """
    Complete intermediate representation for a single validation rule.
    
    RuleIR contains all information needed to emit C code for a validation rule,
    without needing to look up any additional context. This is the "ready to emit"
    representation that decouples parsing from code generation.
    
    The separation between ValidationRule (parsed data) and RuleIR (emission-ready)
    allows us to:
    - Validate rule consistency before emission
    - Add C-specific metadata (types, macros) in one place
    - Test parsing and emission independently
    
    Attributes:
        rule: The original ValidationRule with constraint info
        context: FieldContext with field access information
        c_type_info: C type metadata for numeric rules (None for non-numeric)
        c_macro: The C macro name to use for this rule
        params_formatted: Pre-formatted parameters for C code (e.g., quoted strings)
    """
    rule: ValidationRule
    context: FieldContext
    c_type_info: Optional[CTypeInfo] = None
    c_macro: str = ''
    params_formatted: Dict[str, str] = field(default_factory=dict)
    
    @property
    def rule_type(self) -> str:
        """Shortcut to get the rule type."""
        return self.rule.rule_type
    
    @property
    def constraint_id(self) -> str:
        """Shortcut to get the constraint ID."""
        return self.rule.constraint_id
    
    @property
    def field_name(self) -> str:
        """Shortcut to get the field name."""
        return self.context.field_name
    
    @property
    def field_access(self) -> str:
        """Shortcut to get the field access expression."""
        return self.context.field_access


@dataclass
class FieldRuleSet:
    """
    Aggregates all validation rules for a single field.
    
    FieldRuleSet is the IR for a complete field validation, containing:
    - The field context (regular or oneof)
    - All RuleIR objects for this field
    - Whether recursive submessage validation is needed
    
    Attributes:
        context: FieldContext with field information
        rules: List of RuleIR objects for this field
        needs_submsg_validation: True if this is a message field that needs recursive validation
        submsg_func_name: Name of the validation function for the submessage (if applicable)
    """
    context: FieldContext
    rules: List[RuleIR] = field(default_factory=list)
    needs_submsg_validation: bool = False
    submsg_func_name: str = ''
    
    @property
    def field_name(self) -> str:
        """Shortcut to get the field name."""
        return self.context.field_name
    
    def has_rules(self) -> bool:
        """Return True if this field has any validation rules."""
        return bool(self.rules)


@dataclass
class OneofRuleSet:
    """
    Aggregates all validation rules for a oneof group.
    
    Attributes:
        oneof_name: Name of the oneof group
        oneof_obj: The oneof descriptor object
        member_rule_sets: List of FieldRuleSet for each oneof member with rules
        is_anonymous: True if this is an anonymous oneof (C11 anonymous union)
    """
    oneof_name: str
    oneof_obj: Any
    member_rule_sets: List[FieldRuleSet] = field(default_factory=list)
    is_anonymous: bool = False


@dataclass
class MessageRuleSet:
    """
    Complete validation IR for an entire message.
    
    MessageRuleSet is the top-level IR that contains everything needed to generate
    validation code for a message. It represents the full validation model built
    from parsing, ready for emission.
    
    The pipeline is:
        MessageValidator (parsed rules) → IRBuilder → MessageRuleSet (IR) → CodeEmitter → C code
    
    Attributes:
        message: The message descriptor
        struct_name: The C struct name for this message
        func_name: The validation function name
        field_rule_sets: Rules for regular fields
        oneof_rule_sets: Rules for oneof groups
        message_rules: Message-level validation rules
    """
    message: Any
    struct_name: str
    func_name: str
    field_rule_sets: List[FieldRuleSet] = field(default_factory=list)
    oneof_rule_sets: List[OneofRuleSet] = field(default_factory=list)
    message_rules: List[RuleIR] = field(default_factory=list)


# =============================================================================
# IR BUILDER
# =============================================================================
# The IRBuilder transforms parsed validation rules into the IR representation.
# This separation allows us to validate and preprocess rules before emission.

class IRBuilder:
    """
    Builds RuleIR objects from parsed ValidationRules.
    
    IRBuilder is responsible for transforming the parsed validation rules into
    the emission-ready IR. It:
    - Determines the appropriate C macro for each rule type
    - Resolves C type information for numeric rules
    - Pre-formats parameters for C code generation
    - Creates the complete FieldRuleSet and MessageRuleSet structures
    
    Dependencies:
        - Uses Globals.naming_style for C name formatting
        - Uses CTypeInfo for numeric type resolution
    """
    
    # Mapping from rule type to C macro name for regular fields
    RULE_TO_MACRO = {
        RULE_REQUIRED: 'PB_VALIDATE_REQUIRED',
        RULE_MIN_LEN: 'PB_VALIDATE_STR_MIN_LEN',
        RULE_MAX_LEN: 'PB_VALIDATE_STR_MAX_LEN',
        RULE_PREFIX: 'PB_VALIDATE_STR_PREFIX',
        RULE_SUFFIX: 'PB_VALIDATE_STR_SUFFIX',
        RULE_CONTAINS: 'PB_VALIDATE_STR_CONTAINS',
        RULE_ASCII: 'PB_VALIDATE_STR_ASCII',
        RULE_EMAIL: 'PB_VALIDATE_STR_EMAIL',
        RULE_HOSTNAME: 'PB_VALIDATE_STR_HOSTNAME',
        RULE_IP: 'PB_VALIDATE_STR_IP',
        RULE_IPV4: 'PB_VALIDATE_STR_IPV4',
        RULE_IPV6: 'PB_VALIDATE_STR_IPV6',
        RULE_MIN_ITEMS: 'PB_VALIDATE_MIN_ITEMS',
        RULE_MAX_ITEMS: 'PB_VALIDATE_MAX_ITEMS',
        RULE_UNIQUE: 'PB_VALIDATE_REPEATED_UNIQUE_SCALAR',
        RULE_ENUM_DEFINED: 'PB_VALIDATE_ENUM_DEFINED_ONLY',
        RULE_ANY_IN: 'PB_VALIDATE_ANY_IN',
        RULE_ANY_NOT_IN: 'PB_VALIDATE_ANY_NOT_IN',
        RULE_TIMESTAMP_GT_NOW: 'PB_VALIDATE_TIMESTAMP_GT_NOW',
        RULE_TIMESTAMP_LT_NOW: 'PB_VALIDATE_TIMESTAMP_LT_NOW',
        RULE_TIMESTAMP_WITHIN: 'PB_VALIDATE_TIMESTAMP_WITHIN',
    }
    
    # Numeric rule types that use PB_VALIDATE_NUMERIC_GENERIC
    NUMERIC_COMPARISON_RULES = {RULE_GT, RULE_GTE, RULE_LT, RULE_LTE, RULE_EQ}
    
    # Rule type to C enum constant for numeric comparisons
    NUMERIC_RULE_TO_C_ENUM = {
        RULE_GT: 'PB_VALIDATE_RULE_GT',
        RULE_GTE: 'PB_VALIDATE_RULE_GTE',
        RULE_LT: 'PB_VALIDATE_RULE_LT',
        RULE_LTE: 'PB_VALIDATE_RULE_LTE',
        RULE_EQ: 'PB_VALIDATE_RULE_EQ',
    }
    
    def __init__(self, proto_file: Any):
        """
        Initialize the IR builder.
        
        Args:
            proto_file: The ProtoFile object (used for type lookups)
        """
        self.proto_file = proto_file
    
    def build_rule_ir(self, rule: ValidationRule, context: FieldContext) -> RuleIR:
        """
        Build a RuleIR from a ValidationRule and FieldContext.
        
        Args:
            rule: The parsed ValidationRule
            context: The FieldContext for the field
            
        Returns:
            A complete RuleIR ready for code emission
        """
        # Determine C type info for numeric rules
        c_type_info = None
        if rule.rule_type in self.NUMERIC_COMPARISON_RULES:
            proto_type = rule.constraint_id.split('.', 1)[0]
            c_type_info = CTypeInfo.from_proto_type(proto_type)
        
        # Determine the C macro to use
        c_macro = self._resolve_macro(rule, context)
        
        # Format parameters for C code
        params_formatted = self._format_params(rule)
        
        return RuleIR(
            rule=rule,
            context=context,
            c_type_info=c_type_info,
            c_macro=c_macro,
            params_formatted=params_formatted
        )
    
    def _resolve_macro(self, rule: ValidationRule, context: FieldContext) -> str:
        """Resolve the C macro name for a rule."""
        # Numeric comparisons use NUMERIC_GENERIC
        if rule.rule_type in self.NUMERIC_COMPARISON_RULES:
            if context.is_oneof and not context.is_anonymous:
                return 'PB_VALIDATE_ONEOF_NUMERIC'
            return 'PB_VALIDATE_NUMERIC_GENERIC'
        
        # Lookup in the standard table
        macro = self.RULE_TO_MACRO.get(rule.rule_type, '')
        
        # Adjust for oneof if needed
        if context.is_oneof and not context.is_anonymous and macro:
            # Many macros have ONEOF variants
            oneof_macro = macro.replace('PB_VALIDATE_STR_', 'PB_VALIDATE_ONEOF_STR_')
            if oneof_macro != macro:
                return oneof_macro
        
        return macro
    
    def _format_params(self, rule: ValidationRule) -> Dict[str, str]:
        """Format rule parameters for C code generation."""
        formatted = {}
        params = rule.params
        
        if 'value' in params:
            val = params['value']
            if isinstance(val, str):
                formatted['value'] = '"%s"' % _escape_c_string(val)
            else:
                formatted['value'] = str(val)
        
        if 'values' in params:
            values = params['values']
            if all(isinstance(v, str) for v in values):
                formatted['values_array'] = ', '.join('"%s"' % _escape_c_string(v) for v in values)
            else:
                formatted['values_array'] = ', '.join(str(v) for v in values)
            formatted['values_count'] = str(len(values))
        
        if 'seconds' in params:
            formatted['seconds'] = str(params['seconds'])
        
        if 'field' in params:
            formatted['field'] = str(params['field'])
        
        if 'fields' in params:
            formatted['fields'] = ', '.join(str(f) for f in params['fields'])
        
        if 'n' in params:
            formatted['n'] = str(params['n'])
        
        return formatted
    
    def build_field_rule_set(self, field_validator: 'FieldValidator', 
                             is_oneof: bool = False, 
                             oneof_name: str = '', 
                             is_anonymous: bool = False) -> FieldRuleSet:
        """
        Build a FieldRuleSet from a FieldValidator.
        
        Args:
            field_validator: The parsed FieldValidator
            is_oneof: Whether this field is in a oneof
            oneof_name: Name of the oneof group (if applicable)
            is_anonymous: Whether the oneof is anonymous
            
        Returns:
            A FieldRuleSet with all RuleIRs for this field
        """
        field = field_validator.field
        
        if is_oneof:
            context = FieldContext.for_oneof_member(field, oneof_name, is_anonymous)
        else:
            context = FieldContext.for_regular_field(field)
        
        rule_irs = [self.build_rule_ir(rule, context) for rule in field_validator.rules]
        
        # Check if submessage validation is needed
        needs_submsg = False
        submsg_func = ''
        pbtype = getattr(field, 'pbtype', None)
        if pbtype in ('MESSAGE', 'MSG_W_CB'):
            submsg_ctype = getattr(field, 'ctype', None)
            if submsg_ctype:
                submsg_ctype_str = str(submsg_ctype).lower()
                # Skip google.protobuf types
                if not ('google' in submsg_ctype_str and 'protobuf' in submsg_ctype_str):
                    needs_submsg = True
                    submsg_func = 'pb_validate_' + str(submsg_ctype).replace('.', '_')
        
        return FieldRuleSet(
            context=context,
            rules=rule_irs,
            needs_submsg_validation=needs_submsg,
            submsg_func_name=submsg_func
        )
    
    def build_message_rule_set(self, validator: 'MessageValidator') -> MessageRuleSet:
        """
        Build a complete MessageRuleSet from a MessageValidator.
        
        Args:
            validator: The parsed MessageValidator
            
        Returns:
            A MessageRuleSet with the complete IR for the message
        """
        message = validator.message
        struct_name = str(message.name)
        func_name = 'pb_validate_' + struct_name.replace('.', '_')
        
        # Build field rule sets
        field_rule_sets = []
        for field_name, field_validator in validator.field_validators.items():
            frs = self.build_field_rule_set(field_validator)
            field_rule_sets.append(frs)
        
        # Build oneof rule sets
        oneof_rule_sets = []
        for oneof_name, oneof_data in validator.oneof_validators.items():
            oneof_obj = oneof_data['oneof']
            is_anonymous = getattr(oneof_obj, 'anonymous', False)
            
            member_rule_sets = []
            for member_field, member_fv in oneof_data['members']:
                frs = self.build_field_rule_set(
                    member_fv, 
                    is_oneof=True, 
                    oneof_name=oneof_name,
                    is_anonymous=is_anonymous
                )
                member_rule_sets.append(frs)
            
            oneof_rule_sets.append(OneofRuleSet(
                oneof_name=oneof_name,
                oneof_obj=oneof_obj,
                member_rule_sets=member_rule_sets,
                is_anonymous=is_anonymous
            ))
        
        # Build message rule IRs
        message_rule_irs = []
        for msg_rule in validator.message_rules:
            # Message rules don't have a field context, use dedicated factory
            msg_context = FieldContext.for_message_rule()
            ir = self.build_rule_ir(msg_rule, msg_context)
            message_rule_irs.append(ir)
        
        return MessageRuleSet(
            message=message,
            struct_name=struct_name,
            func_name=func_name,
            field_rule_sets=field_rule_sets,
            oneof_rule_sets=oneof_rule_sets,
            message_rules=message_rule_irs,
        )


class FieldValidator:
    """
    Handles validation rule parsing and storage for a single protobuf field.
    
    This class is responsible for:
    - Extracting validation rules from the field's validate.proto options
    - Converting those options into ValidationRule objects
    - Storing the rules for later code generation
    
    The parsing is type-aware: it knows how to extract rules for strings,
    numeric types, bytes, enums, repeated fields, maps, and special types
    like google.protobuf.Any.
    
    Attributes:
        field: The protobuf field descriptor
        rules: List of ValidationRule objects extracted from the field's options
        proto_file: The ProtoFile object (used for enum lookups, etc.)
    """
    
    def __init__(self, field: Any, rules_option: Any, proto_file: Optional[Any] = None,
                 _message_desc: Optional[Any] = None):
        """
        Initialize a FieldValidator for a given field.
        
        Args:
            field: The protobuf field descriptor
            rules_option: The FieldRules option from validate.proto (or None)
            proto_file: The ProtoFile object containing this field
            _message_desc: Deprecated parameter, retained for API compatibility; no longer used
        """
        self.field = field
        self.rules: List[ValidationRule] = []
        self.proto_file = proto_file
        self.parse_rules(rules_option)
    
    def parse_rules(self, rules_option: Any) -> None:
        """
        Parse validation rules from the FieldRules option.
        
        This is the main entry point for rule extraction. It dispatches to
        type-specific parsers based on which field is set in the FieldRules option.
        
        The method relies only on the provided rules_option (field.validate_rules).
        There is no descriptor scanning or binary parsing fallback.
        
        Args:
            rules_option: The FieldRules message from validate.proto, or None
        """
        if not rules_option:
            return
        
        # Parse type-specific rules
        if rules_option.HasField('string'):
            self._parse_string_rules(rules_option.string)
        if rules_option.HasField('bytes'):
            self._parse_bytes_rules(rules_option.bytes)
        if rules_option.HasField('bool'):
            self._parse_bool_rules(rules_option.bool)
        if rules_option.HasField('enum'):
            self._parse_enum_rules(rules_option.enum)
        if rules_option.HasField('repeated'):
            self._parse_repeated_rules(rules_option.repeated)
        if rules_option.HasField('map'):
            self._parse_map_rules(rules_option.map)
        if rules_option.HasField('any'):
            self._parse_any_rules(rules_option.any)
        if rules_option.HasField('timestamp'):
            self._parse_timestamp_rules(rules_option.timestamp)
        
        # Parse numeric rules for all numeric types (consolidated for clarity)
        numeric_types = [
            'int32', 'int64', 'uint32', 'uint64',
            'sint32', 'sint64', 'fixed32', 'fixed64',
            'sfixed32', 'sfixed64', 'double', 'float'
        ]
        for type_name in numeric_types:
            if rules_option.HasField(type_name):
                self._parse_numeric_rules(getattr(rules_option, type_name), type_name)
        
        # Parse field-level flags
        if rules_option.HasField('required') and rules_option.required:
            self.rules.append(ValidationRule(RULE_REQUIRED, 'required'))
        if rules_option.HasField('oneof_required') and rules_option.oneof_required:
            self.rules.append(ValidationRule(RULE_ONEOF_REQUIRED, 'oneof_required'))
    
    
    def _parse_numeric_rules(self, rules: Any, type_name: str) -> None:
        """
        Parse validation rules for numeric fields (int32, float, etc.).
        
        Numeric rules include comparisons (lt, lte, gt, gte, const) and
        set membership (in, not_in).
        
        Args:
            rules: The numeric rules message (e.g., Int32Rules, FloatRules)
            type_name: The name of the numeric type (e.g., 'int32', 'float')
        """
        if rules.HasField('const_value'):
            self.rules.append(ValidationRule(RULE_EQ, f'{type_name}.const', {'value': rules.const_value}))
        if rules.HasField('lt'):
            self.rules.append(ValidationRule(RULE_LT, f'{type_name}.lt', {'value': rules.lt}))
        if rules.HasField('lte'):
            self.rules.append(ValidationRule(RULE_LTE, f'{type_name}.lte', {'value': rules.lte}))
        if rules.HasField('gt'):
            self.rules.append(ValidationRule(RULE_GT, f'{type_name}.gt', {'value': rules.gt}))
        if rules.HasField('gte'):
            self.rules.append(ValidationRule(RULE_GTE, f'{type_name}.gte', {'value': rules.gte}))
        if hasattr(rules, 'in') and getattr(rules, 'in'):
            self.rules.append(ValidationRule(RULE_IN, f'{type_name}.in', {'values': list(getattr(rules, 'in'))}))
        if rules.not_in:
            self.rules.append(ValidationRule(RULE_NOT_IN, f'{type_name}.not_in', {'values': list(rules.not_in)}))
    
    def _parse_bool_rules(self, rules: Any) -> None:
        """
        Parse validation rules for boolean fields.
        
        Args:
            rules: The BoolRules message from validate.proto
        """
        if rules.HasField('const_value'):
            self.rules.append(ValidationRule(RULE_EQ, 'bool.const', {'value': rules.const_value}))
    
    def _parse_string_rules(self, rules: Any) -> None:
        """
        Parse validation rules for string fields.
        
        String rules include length constraints, pattern matching (prefix, suffix,
        contains), format validation (email, IP address, etc.), and set membership.
        
        Args:
            rules: The StringRules message from validate.proto
        """
        if rules.HasField('const_value'):
            self.rules.append(ValidationRule(RULE_EQ, 'string.const', {'value': rules.const_value}))
        if rules.HasField('min_len'):
            self.rules.append(ValidationRule(RULE_MIN_LEN, 'string.min_len', {'value': rules.min_len}))
        if rules.HasField('max_len'):
            self.rules.append(ValidationRule(RULE_MAX_LEN, 'string.max_len', {'value': rules.max_len}))
        if rules.HasField('prefix'):
            self.rules.append(ValidationRule(RULE_PREFIX, 'string.prefix', {'value': rules.prefix}))
        if rules.HasField('suffix'):
            self.rules.append(ValidationRule(RULE_SUFFIX, 'string.suffix', {'value': rules.suffix}))
        if rules.HasField('contains'):
            self.rules.append(ValidationRule(RULE_CONTAINS, 'string.contains', {'value': rules.contains}))
        if rules.HasField('ascii') and rules.ascii:
            self.rules.append(ValidationRule(RULE_ASCII, 'string.ascii'))
        if getattr(rules, 'email', False):
            self.rules.append(ValidationRule(RULE_EMAIL, 'string.email'))
        if getattr(rules, 'hostname', False):
            self.rules.append(ValidationRule(RULE_HOSTNAME, 'string.hostname'))
        if getattr(rules, 'ip', False):
            self.rules.append(ValidationRule(RULE_IP, 'string.ip'))
        if getattr(rules, 'ipv4', False):
            self.rules.append(ValidationRule(RULE_IPV4, 'string.ipv4'))
        if getattr(rules, 'ipv6', False):
            self.rules.append(ValidationRule(RULE_IPV6, 'string.ipv6'))
        if hasattr(rules, 'in') and getattr(rules, 'in'):
            self.rules.append(ValidationRule(RULE_IN, 'string.in', {'values': list(getattr(rules, 'in'))}))
        if rules.not_in:
            self.rules.append(ValidationRule(RULE_NOT_IN, 'string.not_in', {'values': list(rules.not_in)}))
    
    def _parse_bytes_rules(self, rules: Any) -> None:
        """
        Parse validation rules for bytes fields.
        
        Bytes rules are similar to string rules but operate on byte sequences.
        
        Args:
            rules: The BytesRules message from validate.proto
        """
        if rules.HasField('const_value'):
            self.rules.append(ValidationRule(RULE_EQ, 'bytes.const', {'value': rules.const_value}))
        if rules.HasField('min_len'):
            self.rules.append(ValidationRule(RULE_MIN_LEN, 'bytes.min_len', {'value': rules.min_len}))
        if rules.HasField('max_len'):
            self.rules.append(ValidationRule(RULE_MAX_LEN, 'bytes.max_len', {'value': rules.max_len}))
        if rules.HasField('prefix'):
            self.rules.append(ValidationRule(RULE_PREFIX, 'bytes.prefix', {'value': rules.prefix}))
        if rules.HasField('suffix'):
            self.rules.append(ValidationRule(RULE_SUFFIX, 'bytes.suffix', {'value': rules.suffix}))
        if rules.HasField('contains'):
            self.rules.append(ValidationRule(RULE_CONTAINS, 'bytes.contains', {'value': rules.contains}))
        if hasattr(rules, 'in') and getattr(rules, 'in'):
            self.rules.append(ValidationRule(RULE_IN, 'bytes.in', {'values': list(getattr(rules, 'in'))}))
        if rules.not_in:
            self.rules.append(ValidationRule(RULE_NOT_IN, 'bytes.not_in', {'values': list(rules.not_in)}))
    
    def _parse_enum_rules(self, rules: Any) -> None:
        """
        Parse validation rules for enum fields.
        
        Enum rules include const value, defined_only (must be a known enum value),
        and set membership.
        
        Args:
            rules: The EnumRules message from validate.proto
        """
        if rules.HasField('const_value'):
            self.rules.append(ValidationRule(RULE_EQ, 'enum.const', {'value': rules.const_value}))
        if rules.HasField('defined_only'):
            self.rules.append(ValidationRule(RULE_ENUM_DEFINED, 'enum.defined_only', {'value': rules.defined_only}))
        if hasattr(rules, 'in') and getattr(rules, 'in'):
            self.rules.append(ValidationRule(RULE_IN, 'enum.in', {'values': list(getattr(rules, 'in'))}))
        if rules.not_in:
            self.rules.append(ValidationRule(RULE_NOT_IN, 'enum.not_in', {'values': list(rules.not_in)}))
    
    def _parse_repeated_rules(self, rules: Any) -> None:
        """
        Parse validation rules for repeated fields.
        
        Repeated field rules include item count constraints (min_items, max_items),
        uniqueness constraints, and per-item validation rules.
        
        Args:
            rules: The RepeatedRules message from validate.proto
        """
        if rules.HasField('min_items'):
            self.rules.append(ValidationRule(RULE_MIN_ITEMS, 'repeated.min_items', {'value': rules.min_items}))
        if rules.HasField('max_items'):
            self.rules.append(ValidationRule(RULE_MAX_ITEMS, 'repeated.max_items', {'value': rules.max_items}))
        if rules.HasField('unique') and rules.unique:
            self.rules.append(ValidationRule(RULE_UNIQUE, 'repeated.unique'))
        # Parse per-item validation rules
        if rules.HasField('items'):
            item_rules_list = self._extract_item_rules(rules.items)
            if item_rules_list:
                self.rules.append(ValidationRule(
                    RULE_ITEMS, 'repeated.items', {'item_rules': item_rules_list}
                ))
    
    def _extract_item_rules(self, items_rules: Any) -> List[Dict[str, Any]]:
        """
        Extract per-item validation rules for repeated fields.
        
        This method examines the items rules and extracts type-specific constraints
        that should be applied to each element of the repeated field.
        
        Args:
            items_rules: The repeated.items submessage from validate.proto
            
        Returns:
            A list of dictionaries, each representing one per-item constraint
        """
        result = []
        
        # Extract rules for each type that might be in the items
        if items_rules.HasField('string'):
            result.extend(self._extract_string_item_rules(items_rules.string))
        if items_rules.HasField('bool'):
            result.extend(self._extract_bool_item_rules(items_rules.bool))
        if items_rules.HasField('enum'):
            result.extend(self._extract_enum_item_rules(items_rules.enum))
        
        # Extract numeric item rules for all numeric types (consolidated)
        numeric_types = [
            'int32', 'int64', 'uint32', 'uint64',
            'sint32', 'sint64', 'fixed32', 'fixed64',
            'sfixed32', 'sfixed64', 'float', 'double'
        ]
        for type_name in numeric_types:
            if items_rules.HasField(type_name):
                result.extend(self._extract_numeric_item_rules(
                    getattr(items_rules, type_name), type_name
                ))
        
        return result
    
    def _extract_string_item_rules(self, rules: Any) -> List[Dict[str, Any]]:
        """Extract string validation rules for repeated items."""
        result = []
        if rules.HasField('const_value'):
            result.append({'rule': RULE_EQ, 'constraint_id': 'string.const', 'value': rules.const_value})
        if rules.HasField('min_len'):
            result.append({'rule': RULE_MIN_LEN, 'constraint_id': 'string.min_len', 'value': rules.min_len})
        if rules.HasField('max_len'):
            result.append({'rule': RULE_MAX_LEN, 'constraint_id': 'string.max_len', 'value': rules.max_len})
        if rules.HasField('prefix'):
            result.append({'rule': RULE_PREFIX, 'constraint_id': 'string.prefix', 'value': rules.prefix})
        if rules.HasField('suffix'):
            result.append({'rule': RULE_SUFFIX, 'constraint_id': 'string.suffix', 'value': rules.suffix})
        if rules.HasField('contains'):
            result.append({'rule': RULE_CONTAINS, 'constraint_id': 'string.contains', 'value': rules.contains})
        if rules.HasField('ascii') and rules.ascii:
            result.append({'rule': RULE_ASCII, 'constraint_id': 'string.ascii'})
        if getattr(rules, 'email', False):
            result.append({'rule': RULE_EMAIL, 'constraint_id': 'string.email'})
        if getattr(rules, 'hostname', False):
            result.append({'rule': RULE_HOSTNAME, 'constraint_id': 'string.hostname'})
        if getattr(rules, 'ip', False):
            result.append({'rule': RULE_IP, 'constraint_id': 'string.ip'})
        if getattr(rules, 'ipv4', False):
            result.append({'rule': RULE_IPV4, 'constraint_id': 'string.ipv4'})
        if getattr(rules, 'ipv6', False):
            result.append({'rule': RULE_IPV6, 'constraint_id': 'string.ipv6'})
        if hasattr(rules, 'in') and getattr(rules, 'in'):
            result.append({'rule': RULE_IN, 'constraint_id': 'string.in', 'values': list(getattr(rules, 'in'))})
        if rules.not_in:
            result.append({'rule': RULE_NOT_IN, 'constraint_id': 'string.not_in', 'values': list(rules.not_in)})
        return result
    
    def _extract_numeric_item_rules(self, rules: Any, type_name: str) -> List[Dict[str, Any]]:
        """Extract numeric validation rules for repeated items."""
        result = []
        if rules.HasField('const_value'):
            result.append({'rule': RULE_EQ, 'constraint_id': f'{type_name}.const', 'value': rules.const_value})
        if rules.HasField('lt'):
            result.append({'rule': RULE_LT, 'constraint_id': f'{type_name}.lt', 'value': rules.lt})
        if rules.HasField('lte'):
            result.append({'rule': RULE_LTE, 'constraint_id': f'{type_name}.lte', 'value': rules.lte})
        if rules.HasField('gt'):
            result.append({'rule': RULE_GT, 'constraint_id': f'{type_name}.gt', 'value': rules.gt})
        if rules.HasField('gte'):
            result.append({'rule': RULE_GTE, 'constraint_id': f'{type_name}.gte', 'value': rules.gte})
        if hasattr(rules, 'in') and getattr(rules, 'in'):
            result.append({'rule': RULE_IN, 'constraint_id': f'{type_name}.in', 'values': list(getattr(rules, 'in'))})
        if rules.not_in:
            result.append({'rule': RULE_NOT_IN, 'constraint_id': f'{type_name}.not_in', 'values': list(rules.not_in)})
        return result
    
    def _extract_bool_item_rules(self, rules: Any) -> List[Dict[str, Any]]:
        """Extract bool validation rules for repeated items."""
        result = []
        if rules.HasField('const_value'):
            result.append({'rule': RULE_EQ, 'constraint_id': 'bool.const', 'value': rules.const_value})
        return result
    
    def _extract_enum_item_rules(self, rules: Any) -> List[Dict[str, Any]]:
        """Extract enum validation rules for repeated items."""
        result = []
        if rules.HasField('const_value'):
            result.append({'rule': RULE_EQ, 'constraint_id': 'enum.const', 'value': rules.const_value})
        if rules.HasField('defined_only') and rules.defined_only:
            result.append({'rule': RULE_ENUM_DEFINED, 'constraint_id': 'enum.defined_only', 'value': rules.defined_only})
        if hasattr(rules, 'in') and getattr(rules, 'in'):
            result.append({'rule': RULE_IN, 'constraint_id': 'enum.in', 'values': list(getattr(rules, 'in'))})
        if rules.not_in:
            result.append({'rule': RULE_NOT_IN, 'constraint_id': 'enum.not_in', 'values': list(rules.not_in)})
        return result
    
    def _parse_map_rules(self, rules: Any) -> None:
        """
        Parse validation rules for map fields.
        
        Map rules include min/max pair counts and the no_sparse constraint.
        TODO: Key and value validation rules are not yet implemented.
        
        Args:
            rules: The MapRules message from validate.proto
        """
        if rules.HasField('min_pairs'):
            self.rules.append(ValidationRule(RULE_MIN_ITEMS, 'map.min_pairs', {'value': rules.min_pairs}))
        if rules.HasField('max_pairs'):
            self.rules.append(ValidationRule(RULE_MAX_ITEMS, 'map.max_pairs', {'value': rules.max_pairs}))
        if rules.HasField('no_sparse') and rules.no_sparse:
            self.rules.append(ValidationRule(RULE_NO_SPARSE, 'map.no_sparse'))
        # TODO: Handle key/value validation rules
    
    def _parse_any_rules(self, rules: Any) -> None:
        """
        Parse validation rules for google.protobuf.Any fields.
        
        Any field rules restrict which type URLs are allowed or forbidden.
        
        Args:
            rules: The AnyRules message from validate.proto
        """
        if hasattr(rules, 'in') and getattr(rules, 'in'):
            self.rules.append(ValidationRule(RULE_ANY_IN, 'any.in', {'values': list(getattr(rules, 'in'))}))
        if hasattr(rules, 'not_in') and getattr(rules, 'not_in'):
            self.rules.append(ValidationRule(RULE_ANY_NOT_IN, 'any.not_in', {'values': list(getattr(rules, 'not_in'))}))
    
    def _parse_timestamp_rules(self, rules: Any) -> None:
        """
        Parse validation rules for google.protobuf.Timestamp fields.
        
        Timestamp field rules can check if timestamp is before/after current time
        or within a specified duration from now.
        
        Args:
            rules: The TimestampRules message from validate.proto
        """
        if hasattr(rules, 'gt_now') and rules.gt_now:
            self.rules.append(ValidationRule(RULE_TIMESTAMP_GT_NOW, 'timestamp.gt_now'))
        if hasattr(rules, 'lt_now') and rules.lt_now:
            self.rules.append(ValidationRule(RULE_TIMESTAMP_LT_NOW, 'timestamp.lt_now'))
        if hasattr(rules, 'within') and rules.HasField('within'):
            seconds = rules.within.seconds if hasattr(rules.within, 'seconds') else 0
            self.rules.append(ValidationRule(RULE_TIMESTAMP_WITHIN, 'timestamp.within', {'seconds': seconds}))
class MessageValidator:
    """
    Handles validation for an entire protobuf message.
    
    This class aggregates:
    - Field validators for regular fields
    - Oneof validators for oneof groups
    - Message-level validation rules
    
    It coordinates the parsing of all validation constraints for a message
    and makes them available for code generation.
    
    Attributes:
        message: The protobuf message descriptor
        field_validators: OrderedDict mapping field names to FieldValidator objects
        oneof_validators: OrderedDict mapping oneof names to their validators
        message_rules: List of message-level ValidationRule objects
        proto_file: The ProtoFile object containing this message
    """
    
    def __init__(self, message: Any, message_rules: Optional[Any] = None,
                 proto_file: Optional[Any] = None):
        """
        Initialize a MessageValidator for a given message.
        
        Args:
            message: The protobuf message descriptor
            message_rules: The MessageRules option from validate.proto (or None)
            proto_file: The ProtoFile object containing this message
        """
        self.message = message
        self.field_validators = OrderedDict()
        self.oneof_validators = OrderedDict()
        self.message_rules: List[ValidationRule] = []
        self.proto_file = proto_file
        
        # Parse field-level and oneof rules
        self._parse_field_validators()
        
        # Parse message-level rules
        if message_rules:
            self.parse_message_rules(message_rules)
    
    def _parse_field_validators(self) -> None:
        """
        Parse validation rules for all fields in the message.
        
        This method iterates through all fields, identifies oneofs, and creates
        FieldValidator objects for fields that have validation rules.
        """
        for field in getattr(self.message, 'fields', []) or []:
            # Check if this is a oneof container
            if hasattr(field, 'pbtype') and field.pbtype == 'oneof':
                self._parse_oneof_field(field)
            elif hasattr(field, 'validate_rules') and field.validate_rules:
                # Regular field with validation rules
                fv = FieldValidator(
                    field, field.validate_rules, self.proto_file, self.message
                )
                self.field_validators[field.name] = fv
    
    def _parse_oneof_field(self, oneof: Any) -> None:
        """
        Parse validation rules for fields within a oneof group.
        
        Args:
            oneof: The oneof field descriptor
        """
        oneof_name = oneof.name
        oneof_fields_with_rules = []
        
        for oneof_member in getattr(oneof, 'fields', []) or []:
            if hasattr(oneof_member, 'validate_rules') and oneof_member.validate_rules:
                fv = FieldValidator(
                    oneof_member, oneof_member.validate_rules,
                    self.proto_file, self.message
                )
                oneof_fields_with_rules.append((oneof_member, fv))
        
        if oneof_fields_with_rules:
            self.oneof_validators[oneof_name] = {
                'oneof': oneof,
                'members': oneof_fields_with_rules
            }
    
    def parse_message_rules(self, rules: Any) -> None:
        """
        Parse message-level validation rules.
        
        Message-level rules include:
        - requires: Fields that must be present
        - mutex: Fields that are mutually exclusive
        - at_least: At least N of a set of fields must be present
        
        Args:
            rules: The MessageRules option from validate.proto
        """
        if rules.requires:
            for field_name in rules.requires:
                self.message_rules.append(ValidationRule('REQUIRES', 'message.requires', {'field': field_name}))
        
        if rules.mutex:
            for group in rules.mutex:
                self.message_rules.append(ValidationRule('MUTEX', 'message.mutex', {'fields': list(group.fields)}))
        
        if rules.at_least:
            for rule in rules.at_least:
                self.message_rules.append(ValidationRule('AT_LEAST', 'message.at_least', 
                                                       {'n': rule.n, 'fields': list(rule.fields)}))


# =============================================================================
# RULE EMITTERS
# =============================================================================
# RuleEmitter classes handle the emission of C code for validation rules.
# Each emitter handles a category of rules and produces the appropriate C code.

from abc import ABC, abstractmethod

class RuleEmitter(ABC):
    """
    Abstract base class for rule emitters.
    
    RuleEmitters convert RuleIR objects into C code strings. Each concrete
    emitter handles a category of validation rules (e.g., numeric, string, repeated).
    
    The table-driven dispatch in RuleEmitterRegistry routes each rule to its
    appropriate emitter based on rule type.
    """
    
    @abstractmethod
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        """
        Emit C code for a validation rule.
        
        Args:
            rule_ir: The RuleIR containing all emission context
            proto_file: The ProtoFile (for enum lookups, etc.)
            
        Returns:
            A string of C code implementing the validation
        """
        pass


class NumericRuleEmitter(RuleEmitter):
    """Emits C code for numeric comparison rules (GT, GTE, LT, LTE, EQ)."""
    
    RULE_TO_C_ENUM = {
        RULE_GT: 'PB_VALIDATE_RULE_GT',
        RULE_GTE: 'PB_VALIDATE_RULE_GTE',
        RULE_LT: 'PB_VALIDATE_RULE_LT',
        RULE_LTE: 'PB_VALIDATE_RULE_LTE',
        RULE_EQ: 'PB_VALIDATE_RULE_EQ',
    }
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        if not rule_ir.c_type_info:
            return ''
        
        rule_enum = self.RULE_TO_C_ENUM.get(rule_ir.rule_type, '')
        if not rule_enum:
            return ''
        
        value = rule_ir.rule.params.get('value', 0)
        ctype = rule_ir.c_type_info.c_type
        validator_func = rule_ir.c_type_info.validator_func
        
        if rule_ir.context.is_oneof and not rule_ir.context.is_anonymous:
            return (
                '    PB_VALIDATE_ONEOF_NUMERIC(ctx, msg, %s, %s, %s, %s, %s, %s, "%s");\n'
            ) % (rule_ir.context.oneof_name, rule_ir.field_name, ctype, 
                 validator_func, rule_enum, value, rule_ir.constraint_id)
        else:
            return (
                '        PB_VALIDATE_NUMERIC_GENERIC(ctx, msg, %s, %s, %s, %s, %s, "%s");\n'
            ) % (rule_ir.field_name, ctype, validator_func, rule_enum, value, rule_ir.constraint_id)


class StringLengthRuleEmitter(RuleEmitter):
    """Emits C code for string length rules (MIN_LEN, MAX_LEN)."""
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        is_min = rule_ir.rule_type == RULE_MIN_LEN
        length = rule_ir.rule.params.get('value', 0)
        field_name = rule_ir.field_name
        ctx = rule_ir.context
        
        is_string = 'string' in rule_ir.constraint_id
        
        if ctx.is_oneof and not ctx.is_anonymous:
            if is_string:
                macro = 'PB_VALIDATE_ONEOF_STR_MIN_LEN' if is_min else 'PB_VALIDATE_ONEOF_STR_MAX_LEN'
                return '    %s(ctx, msg, %s, %s, %d, "%s");\n' % (
                    macro, ctx.oneof_name, field_name, length, rule_ir.constraint_id)
            else:  # bytes
                macro = 'PB_VALIDATE_ONEOF_BYTES_MIN_LEN' if is_min else 'PB_VALIDATE_ONEOF_BYTES_MAX_LEN'
                return '    %s(ctx, msg, %s, %s, %d, "%s");\n' % (
                    macro, ctx.oneof_name, field_name, length, rule_ir.constraint_id)
        else:
            if is_string:
                macro = 'PB_VALIDATE_STR_MIN_LEN' if is_min else 'PB_VALIDATE_STR_MAX_LEN'
            else:
                macro = 'PB_VALIDATE_BYTES_MIN_LEN' if is_min else 'PB_VALIDATE_BYTES_MAX_LEN'
            return '        %s(ctx, msg, %s, %d, "%s");\n' % (
                macro, field_name, length, rule_ir.constraint_id)


class StringFormatRuleEmitter(RuleEmitter):
    """Emits C code for string format rules (EMAIL, HOSTNAME, IP, etc.)."""
    
    NORMAL_MACROS = {
        RULE_EMAIL: 'PB_VALIDATE_STR_EMAIL',
        RULE_HOSTNAME: 'PB_VALIDATE_STR_HOSTNAME',
        RULE_IP: 'PB_VALIDATE_STR_IP',
        RULE_IPV4: 'PB_VALIDATE_STR_IPV4',
        RULE_IPV6: 'PB_VALIDATE_STR_IPV6',
        RULE_ASCII: 'PB_VALIDATE_STR_ASCII',
    }
    
    RULE_TO_C_ENUM = {
        RULE_EMAIL: 'PB_VALIDATE_RULE_EMAIL',
        RULE_HOSTNAME: 'PB_VALIDATE_RULE_HOSTNAME',
        RULE_IP: 'PB_VALIDATE_RULE_IP',
        RULE_IPV4: 'PB_VALIDATE_RULE_IPV4',
        RULE_IPV6: 'PB_VALIDATE_RULE_IPV6',
        RULE_ASCII: 'PB_VALIDATE_RULE_ASCII',
    }
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        ctx = rule_ir.context
        field_name = rule_ir.field_name
        
        if ctx.is_oneof and not ctx.is_anonymous:
            rule_enum = self.RULE_TO_C_ENUM.get(rule_ir.rule_type, '')
            if not rule_enum:
                return ''
            return '    PB_VALIDATE_ONEOF_STR_FORMAT(ctx, msg, %s, %s, %s, "%s", "String format validation failed");\n' % (
                ctx.oneof_name, field_name, rule_enum, rule_ir.constraint_id)
        else:
            macro = self.NORMAL_MACROS.get(rule_ir.rule_type, '')
            if not macro:
                return ''
            return '        %s(ctx, msg, %s, "%s");\n' % (macro, field_name, rule_ir.constraint_id)


class StringPatternRuleEmitter(RuleEmitter):
    """Emits C code for string pattern rules (PREFIX, SUFFIX, CONTAINS)."""
    
    NORMAL_MACROS = {
        RULE_PREFIX: 'PB_VALIDATE_STR_PREFIX',
        RULE_SUFFIX: 'PB_VALIDATE_STR_SUFFIX',
        RULE_CONTAINS: 'PB_VALIDATE_STR_CONTAINS',
    }
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        ctx = rule_ir.context
        field_name = rule_ir.field_name
        pattern = _escape_c_string(rule_ir.rule.params.get('value', ''))
        
        if ctx.is_oneof and not ctx.is_anonymous:
            oneof_macro = {
                RULE_PREFIX: 'PB_VALIDATE_ONEOF_STR_PREFIX',
                RULE_SUFFIX: 'PB_VALIDATE_ONEOF_STR_SUFFIX',
                RULE_CONTAINS: 'PB_VALIDATE_ONEOF_STR_CONTAINS',
            }.get(rule_ir.rule_type, '')
            if oneof_macro:
                return '    %s(ctx, msg, %s, %s, "%s", "%s");\n' % (
                    oneof_macro, ctx.oneof_name, field_name, pattern, rule_ir.constraint_id)
        else:
            macro = self.NORMAL_MACROS.get(rule_ir.rule_type, '')
            if macro:
                return '        %s(ctx, msg, %s, "%s", "%s");\n' % (
                    macro, field_name, pattern, rule_ir.constraint_id)
        return ''


class RepeatedRuleEmitter(RuleEmitter):
    """Emits C code for repeated field rules (MIN_ITEMS, MAX_ITEMS, UNIQUE)."""
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        field_name = rule_ir.field_name
        
        if rule_ir.rule_type == RULE_MIN_ITEMS:
            value = rule_ir.rule.params.get('value', 0)
            return '        PB_VALIDATE_MIN_ITEMS(ctx, msg, %s, %d, "%s");\n' % (
                field_name, value, rule_ir.constraint_id)
        
        elif rule_ir.rule_type == RULE_MAX_ITEMS:
            value = rule_ir.rule.params.get('value', 0)
            return '        PB_VALIDATE_MAX_ITEMS(ctx, msg, %s, %d, "%s");\n' % (
                field_name, value, rule_ir.constraint_id)
        
        elif rule_ir.rule_type == RULE_UNIQUE:
            pbtype = getattr(rule_ir.context.field, 'pbtype', None)
            if pbtype in ('MESSAGE', 'MSG_W_CB'):
                return '        /* NOTE: repeated.unique is not supported for message types */\n'
            elif pbtype == 'STRING':
                return '        PB_VALIDATE_REPEATED_UNIQUE_STRING(ctx, msg, %s, "%s");\n' % (
                    field_name, rule_ir.constraint_id)
            elif pbtype == 'BYTES':
                return '        PB_VALIDATE_REPEATED_UNIQUE_BYTES(ctx, msg, %s, "%s");\n' % (
                    field_name, rule_ir.constraint_id)
            else:
                return '        PB_VALIDATE_REPEATED_UNIQUE_SCALAR(ctx, msg, %s, "%s");\n' % (
                    field_name, rule_ir.constraint_id)
        
        return ''


class ItemsRuleEmitter(RuleEmitter):
    """
    Emits C code for per-item validation on repeated fields (RULE_ITEMS).
    
    This emitter generates code that iterates over each element in a repeated
    field and applies the specified constraints (min_len, max_len, gt, lt, etc.).
    """
    
    # Mapping from rule types to C enum constants for numeric comparisons
    NUMERIC_RULE_TO_C_ENUM = {
        RULE_GT: 'PB_VALIDATE_RULE_GT',
        RULE_GTE: 'PB_VALIDATE_RULE_GTE',
        RULE_LT: 'PB_VALIDATE_RULE_LT',
        RULE_LTE: 'PB_VALIDATE_RULE_LTE',
        RULE_EQ: 'PB_VALIDATE_RULE_EQ',
    }
    
    # Mapping from string format rule types to C enum constants
    STRING_FORMAT_RULE_TO_C_ENUM = {
        RULE_EMAIL: 'PB_VALIDATE_RULE_EMAIL',
        RULE_HOSTNAME: 'PB_VALIDATE_RULE_HOSTNAME',
        RULE_IP: 'PB_VALIDATE_RULE_IP',
        RULE_IPV4: 'PB_VALIDATE_RULE_IPV4',
        RULE_IPV6: 'PB_VALIDATE_RULE_IPV6',
        RULE_ASCII: 'PB_VALIDATE_RULE_ASCII',
    }
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        """
        Emit C code for per-item validation on a repeated field.
        
        Args:
            rule_ir: The RuleIR containing item_rules in params
            proto_file: The ProtoFile for context
            
        Returns:
            C code string implementing per-item validation
        """
        field_name = rule_ir.field_name
        item_rules = rule_ir.rule.params.get('item_rules', [])
        if not item_rules:
            return ''
        
        field = rule_ir.context.field
        pbtype = getattr(field, 'pbtype', None)
        code = ''
        
        for item_rule in item_rules:
            rule_type = item_rule.get('rule')
            constraint_id = item_rule.get('constraint_id', '')
            
            if rule_type == RULE_MIN_LEN and pbtype == 'STRING':
                value = item_rule.get('value', 0)
                code += '        PB_VALIDATE_REPEATED_ITEMS_STR_MIN_LEN(ctx, msg, %s, %d, "%s");\n' % (
                    field_name, value, constraint_id)
            
            elif rule_type == RULE_MAX_LEN and pbtype == 'STRING':
                value = item_rule.get('value', 0)
                code += '        PB_VALIDATE_REPEATED_ITEMS_STR_MAX_LEN(ctx, msg, %s, %d, "%s");\n' % (
                    field_name, value, constraint_id)
            
            elif rule_type in (RULE_GT, RULE_GTE, RULE_LT, RULE_LTE, RULE_EQ):
                value = item_rule.get('value', 0)
                # Extract just the type name from the constraint_id (e.g., 'int32' from 'int32.gt')
                type_name = constraint_id.split('.', 1)[0]
                ctype, func = _get_numeric_validator_info(type_name)
                if ctype and func:
                    rule_enum = self.NUMERIC_RULE_TO_C_ENUM.get(rule_type)
                    if rule_enum:
                        code += '        PB_VALIDATE_REPEATED_ITEMS_NUMERIC(ctx, msg, %s, %s, %s, %s, %s, "%s");\n' % (
                            field_name, ctype, func, rule_enum, value, constraint_id)
            
            elif rule_type == RULE_PREFIX and pbtype == 'STRING':
                prefix = item_rule.get('value', '')
                code += '        /* repeated.items string.prefix validation */\n'
                code += '        for (pb_size_t __pb_i = 0; __pb_i < msg->%s_count; ++__pb_i) {\n' % field_name
                code += '            pb_validate_context_push_index(&ctx, __pb_i);\n'
                code += '            const char *__pb_prefix = "%s";\n' % prefix
                code += '            if (!pb_validate_string(msg->%s[__pb_i], (pb_size_t)strlen(msg->%s[__pb_i]), __pb_prefix, PB_VALIDATE_RULE_PREFIX)) {\n' % (field_name, field_name)
                code += '                pb_violations_add(violations, ctx.path_buffer, "%s", "String must start with specified prefix");\n' % constraint_id
                code += '                if (ctx.early_exit) { pb_validate_context_pop_index(&ctx); return false; }\n'
                code += '            }\n'
                code += '            pb_validate_context_pop_index(&ctx);\n'
                code += '        }\n'
            
            elif rule_type == RULE_SUFFIX and pbtype == 'STRING':
                suffix = item_rule.get('value', '')
                code += '        /* repeated.items string.suffix validation */\n'
                code += '        for (pb_size_t __pb_i = 0; __pb_i < msg->%s_count; ++__pb_i) {\n' % field_name
                code += '            pb_validate_context_push_index(&ctx, __pb_i);\n'
                code += '            const char *__pb_suffix = "%s";\n' % suffix
                code += '            if (!pb_validate_string(msg->%s[__pb_i], (pb_size_t)strlen(msg->%s[__pb_i]), __pb_suffix, PB_VALIDATE_RULE_SUFFIX)) {\n' % (field_name, field_name)
                code += '                pb_violations_add(violations, ctx.path_buffer, "%s", "String must end with specified suffix");\n' % constraint_id
                code += '                if (ctx.early_exit) { pb_validate_context_pop_index(&ctx); return false; }\n'
                code += '            }\n'
                code += '            pb_validate_context_pop_index(&ctx);\n'
                code += '        }\n'
            
            elif rule_type == RULE_CONTAINS and pbtype == 'STRING':
                needle = item_rule.get('value', '')
                code += '        /* repeated.items string.contains validation */\n'
                code += '        for (pb_size_t __pb_i = 0; __pb_i < msg->%s_count; ++__pb_i) {\n' % field_name
                code += '            pb_validate_context_push_index(&ctx, __pb_i);\n'
                code += '            const char *__pb_needle = "%s";\n' % needle
                code += '            if (!pb_validate_string(msg->%s[__pb_i], (pb_size_t)strlen(msg->%s[__pb_i]), __pb_needle, PB_VALIDATE_RULE_CONTAINS)) {\n' % (field_name, field_name)
                code += '                pb_violations_add(violations, ctx.path_buffer, "%s", "String must contain specified substring");\n' % constraint_id
                code += '                if (ctx.early_exit) { pb_validate_context_pop_index(&ctx); return false; }\n'
                code += '            }\n'
                code += '            pb_validate_context_pop_index(&ctx);\n'
                code += '        }\n'
            
            elif rule_type == RULE_ASCII and pbtype == 'STRING':
                code += '        /* repeated.items string.ascii validation */\n'
                code += '        for (pb_size_t __pb_i = 0; __pb_i < msg->%s_count; ++__pb_i) {\n' % field_name
                code += '            pb_validate_context_push_index(&ctx, __pb_i);\n'
                code += '            if (!pb_validate_string(msg->%s[__pb_i], (pb_size_t)strlen(msg->%s[__pb_i]), NULL, PB_VALIDATE_RULE_ASCII)) {\n' % (field_name, field_name)
                code += '                pb_violations_add(violations, ctx.path_buffer, "%s", "String must contain only ASCII characters");\n' % constraint_id
                code += '                if (ctx.early_exit) { pb_validate_context_pop_index(&ctx); return false; }\n'
                code += '            }\n'
                code += '            pb_validate_context_pop_index(&ctx);\n'
                code += '        }\n'
            
            elif rule_type in (RULE_EMAIL, RULE_HOSTNAME, RULE_IP, RULE_IPV4, RULE_IPV6) and pbtype == 'STRING':
                rule_enum = self.STRING_FORMAT_RULE_TO_C_ENUM.get(rule_type)
                if rule_enum:
                    code += '        /* repeated.items %s validation */\n' % constraint_id
                    code += '        for (pb_size_t __pb_i = 0; __pb_i < msg->%s_count; ++__pb_i) {\n' % field_name
                    code += '            pb_validate_context_push_index(&ctx, __pb_i);\n'
                    code += '            if (!pb_validate_string(msg->%s[__pb_i], (pb_size_t)strlen(msg->%s[__pb_i]), NULL, %s)) {\n' % (field_name, field_name, rule_enum)
                    code += '                pb_violations_add(violations, ctx.path_buffer, "%s", "String format validation failed");\n' % constraint_id
                    code += '                if (ctx.early_exit) { pb_validate_context_pop_index(&ctx); return false; }\n'
                    code += '            }\n'
                    code += '            pb_validate_context_pop_index(&ctx);\n'
                    code += '        }\n'
            
            elif rule_type == RULE_IN:
                values = item_rule.get('values', [])
                if pbtype == 'STRING':
                    values_array = ', '.join('"%s"' % v for v in values)
                    code += '        /* repeated.items string.in validation */\n'
                    code += '        for (pb_size_t __pb_i = 0; __pb_i < msg->%s_count; ++__pb_i) {\n' % field_name
                    code += '            pb_validate_context_push_index(&ctx, __pb_i);\n'
                    code += '            const char *__pb_allowed[] = { %s };\n' % values_array
                    code += '            bool __pb_match = false;\n'
                    code += '            for (size_t __pb_k = 0; __pb_k < sizeof(__pb_allowed)/sizeof(__pb_allowed[0]); __pb_k++) {\n'
                    code += '                if (strcmp(msg->%s[__pb_i], __pb_allowed[__pb_k]) == 0) { __pb_match = true; break; }\n' % field_name
                    code += '            }\n'
                    code += '            if (!__pb_match) {\n'
                    code += '                pb_violations_add(violations, ctx.path_buffer, "%s", "Value must be one of allowed set");\n' % constraint_id
                    code += '                if (ctx.early_exit) { pb_validate_context_pop_index(&ctx); return false; }\n'
                    code += '            }\n'
                    code += '            pb_validate_context_pop_index(&ctx);\n'
                    code += '        }\n'
                else:
                    conditions = ['msg->%s[__pb_i] == %s' % (field_name, v) for v in values]
                    condition_str = ' || '.join(conditions) if conditions else 'false'
                    code += '        /* repeated.items %s.in validation */\n' % constraint_id.split('.')[0]
                    code += '        for (pb_size_t __pb_i = 0; __pb_i < msg->%s_count; ++__pb_i) {\n' % field_name
                    code += '            pb_validate_context_push_index(&ctx, __pb_i);\n'
                    code += '            if (!(%s)) {\n' % condition_str
                    code += '                pb_violations_add(violations, ctx.path_buffer, "%s", "Value must be one of allowed set");\n' % constraint_id
                    code += '                if (ctx.early_exit) { pb_validate_context_pop_index(&ctx); return false; }\n'
                    code += '            }\n'
                    code += '            pb_validate_context_pop_index(&ctx);\n'
                    code += '        }\n'
            
            elif rule_type == RULE_NOT_IN:
                values = item_rule.get('values', [])
                if pbtype == 'STRING':
                    values_array = ', '.join('"%s"' % v for v in values)
                    code += '        /* repeated.items string.not_in validation */\n'
                    code += '        for (pb_size_t __pb_i = 0; __pb_i < msg->%s_count; ++__pb_i) {\n' % field_name
                    code += '            pb_validate_context_push_index(&ctx, __pb_i);\n'
                    code += '            const char *__pb_blocked[] = { %s };\n' % values_array
                    code += '            bool __pb_forbidden = false;\n'
                    code += '            for (size_t __pb_k = 0; __pb_k < sizeof(__pb_blocked)/sizeof(__pb_blocked[0]); __pb_k++) {\n'
                    code += '                if (strcmp(msg->%s[__pb_i], __pb_blocked[__pb_k]) == 0) { __pb_forbidden = true; break; }\n' % field_name
                    code += '            }\n'
                    code += '            if (__pb_forbidden) {\n'
                    code += '                pb_violations_add(violations, ctx.path_buffer, "%s", "Value is in forbidden set");\n' % constraint_id
                    code += '                if (ctx.early_exit) { pb_validate_context_pop_index(&ctx); return false; }\n'
                    code += '            }\n'
                    code += '            pb_validate_context_pop_index(&ctx);\n'
                    code += '        }\n'
                else:
                    conditions = ['msg->%s[__pb_i] != %s' % (field_name, v) for v in values]
                    condition_str = ' && '.join(conditions) if conditions else 'true'
                    code += '        /* repeated.items %s.not_in validation */\n' % constraint_id.split('.')[0]
                    code += '        for (pb_size_t __pb_i = 0; __pb_i < msg->%s_count; ++__pb_i) {\n' % field_name
                    code += '            pb_validate_context_push_index(&ctx, __pb_i);\n'
                    code += '            if (!(%s)) {\n' % condition_str
                    code += '                pb_violations_add(violations, ctx.path_buffer, "%s", "Value is in forbidden set");\n' % constraint_id
                    code += '                if (ctx.early_exit) { pb_validate_context_pop_index(&ctx); return false; }\n'
                    code += '            }\n'
                    code += '            pb_validate_context_pop_index(&ctx);\n'
                    code += '        }\n'
        
        return code


class TimestampRuleEmitter(RuleEmitter):
    """Emits C code for timestamp rules (GT_NOW, LT_NOW, WITHIN)."""
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        field_name = rule_ir.field_name
        
        if rule_ir.rule_type == RULE_TIMESTAMP_GT_NOW:
            return '    PB_VALIDATE_TIMESTAMP_GT_NOW(ctx, msg, %s, "%s");\n' % (
                field_name, rule_ir.constraint_id)
        
        elif rule_ir.rule_type == RULE_TIMESTAMP_LT_NOW:
            return '    PB_VALIDATE_TIMESTAMP_LT_NOW(ctx, msg, %s, "%s");\n' % (
                field_name, rule_ir.constraint_id)
        
        elif rule_ir.rule_type == RULE_TIMESTAMP_WITHIN:
            seconds = rule_ir.rule.params.get('seconds', 0)
            return '    PB_VALIDATE_TIMESTAMP_WITHIN(ctx, msg, %s, %d, "%s");\n' % (
                field_name, seconds, rule_ir.constraint_id)
        
        return ''


class AnyRuleEmitter(RuleEmitter):
    """Emits C code for Any field rules (ANY_IN, ANY_NOT_IN)."""
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        field_name = rule_ir.field_name
        values = rule_ir.rule.params.get('values', [])
        
        if not values:
            return ''
        
        values_array = ', '.join('"%s"' % url for url in values)
        
        if rule_ir.rule_type == RULE_ANY_IN:
            return ('    static const char *__pb_%s_urls_in[] = { %s };\n'
                    '    PB_VALIDATE_ANY_IN(ctx, msg, %s, __pb_%s_urls_in, %d, "%s");\n') % (
                field_name, values_array, field_name, field_name, len(values), rule_ir.constraint_id)
        
        elif rule_ir.rule_type == RULE_ANY_NOT_IN:
            return ('    static const char *__pb_%s_urls_notin[] = { %s };\n'
                    '    PB_VALIDATE_ANY_NOT_IN(ctx, msg, %s, __pb_%s_urls_notin, %d, "%s");\n') % (
                field_name, values_array, field_name, field_name, len(values), rule_ir.constraint_id)
        
        return ''


class RequiredRuleEmitter(RuleEmitter):
    """Emits C code for required field rule."""
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        field = rule_ir.context.field
        if getattr(field, 'rules', None) == 'OPTIONAL':
            return ('        if (!msg->has_%s) {\n'
                    '            pb_violations_add(violations, ctx.path_buffer, "%s", "Field is required");\n'
                    '            if (ctx.early_exit) return false;\n'
                    '        }\n') % (rule_ir.field_name, rule_ir.constraint_id)
        return ''


class InNotInRuleEmitter(RuleEmitter):
    """Emits C code for IN and NOT_IN rules."""
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        values = rule_ir.rule.params.get('values', [])
        field_name = rule_ir.field_name
        ctx = rule_ir.context
        is_in = rule_ir.rule_type == RULE_IN
        
        is_string = 'string' in rule_ir.constraint_id
        
        if is_string:
            values_array = ', '.join('"%s"' % _escape_c_string(v) for v in values)
            
            if ctx.is_oneof and not ctx.is_anonymous:
                field_access = '%s.%s' % (ctx.oneof_name, field_name)
                if is_in:
                    conditions = ['strcmp(msg->%s, "%s") == 0' % (field_access, _escape_c_string(v)) for v in values]
                    condition_str = ' || '.join(conditions)
                    return ('    if (!(%s)) { pb_violations_add(violations, ctx.path_buffer, "%s", "Value must be one of allowed set"); if (ctx.early_exit) return false; }\n'
                           ) % (condition_str, rule_ir.constraint_id)
                else:
                    conditions = ['strcmp(msg->%s, "%s") != 0' % (field_access, _escape_c_string(v)) for v in values]
                    condition_str = ' && '.join(conditions)
                    return ('    if (!(%s)) { pb_violations_add(violations, ctx.path_buffer, "%s", "Value in forbidden set"); if (ctx.early_exit) return false; }\n'
                           ) % (condition_str, rule_ir.constraint_id)
            else:
                if is_in:
                    return ('    static const char *__pb_%s_in[] = { %s };\n'
                            '    PB_VALIDATE_STR_IN(ctx, msg, %s, __pb_%s_in, %d, "%s");\n') % (
                        field_name, values_array, field_name, field_name, len(values), rule_ir.constraint_id)
                else:
                    return ('    static const char *__pb_%s_notin[] = { %s };\n'
                            '    PB_VALIDATE_STR_NOT_IN(ctx, msg, %s, __pb_%s_notin, %d, "%s");\n') % (
                        field_name, values_array, field_name, field_name, len(values), rule_ir.constraint_id)
        else:
            # Numeric IN/NOT_IN
            field_access = field_name if not (ctx.is_oneof and not ctx.is_anonymous) else '%s.%s' % (ctx.oneof_name, field_name)
            
            if is_in:
                conditions = ['msg->%s == %s' % (field_access, v) for v in values]
                condition_str = ' || '.join(conditions)
                values_str = ', '.join(str(v) for v in values)
                return ('        if (!(%s)) {\n'
                        '            pb_violations_add(violations, ctx.path_buffer, "%s", "Value must be one of: %s");\n'
                        '            if (ctx.early_exit) return false;\n'
                        '        }\n') % (condition_str, rule_ir.constraint_id, values_str)
            else:
                conditions = ['msg->%s != %s' % (field_access, v) for v in values]
                condition_str = ' && '.join(conditions)
                values_str = ', '.join(str(v) for v in values)
                return ('        if (!(%s)) {\n'
                        '            pb_violations_add(violations, ctx.path_buffer, "%s", "Value must not be one of: %s");\n'
                        '            if (ctx.early_exit) return false;\n'
                        '        }\n') % (condition_str, rule_ir.constraint_id, values_str)


class EnumDefinedRuleEmitter(RuleEmitter):
    """Emits C code for enum defined_only rule."""
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        if not rule_ir.rule.params.get('value', True):
            return ''
        
        try:
            enum_vals = []
            field = rule_ir.context.field
            ctype = getattr(field, 'ctype', None)
            
            for e in getattr(proto_file, 'enums', []) or []:
                try:
                    if e.names == ctype or str(e.names) == str(ctype):
                        enum_vals = [int(v) for (_, v) in getattr(e, 'values', [])]
                        break
                except Exception:
                    continue
            
            if enum_vals:
                arr_values = ', '.join(str(v) for v in enum_vals)
                field_name = rule_ir.field_name
                ctx = rule_ir.context
                
                if ctx.is_oneof and not ctx.is_anonymous:
                    field_access = '%s.%s' % (ctx.oneof_name, field_name)
                    return ('    static const int __pb_%s_vals[] = { %s };\n'
                            '    if (!pb_validate_enum_defined_only((int)msg->%s, __pb_%s_vals, (pb_size_t)(sizeof(__pb_%s_vals)/sizeof(__pb_%s_vals[0])))) {\n'
                            '        pb_violations_add(violations, ctx.path_buffer, "%s", "Value must be a defined enum value");\n'
                            '        if (ctx.early_exit) return false;\n'
                            '    }\n') % (field_name, arr_values, field_access, field_name, field_name, field_name, rule_ir.constraint_id)
                else:
                    return ('    static const int __pb_%s_vals[] = { %s };\n'
                            '    PB_VALIDATE_ENUM_DEFINED_ONLY(ctx, msg, %s, __pb_%s_vals, "%s");\n'
                           ) % (field_name, arr_values, field_name, field_name, rule_ir.constraint_id)
        except Exception:
            pass
        
        return ''


class RuleEmitterRegistry:
    """
    Registry for rule emitters with table-driven dispatch.
    
    This class provides a central dispatch mechanism for routing rules to their
    appropriate emitters. Adding a new rule type requires:
    1. Creating a RuleEmitter subclass
    2. Registering it in this registry
    
    This is the single place where rule → emitter mapping is defined.
    """
    
    def __init__(self):
        """Initialize the registry with all known emitters."""
        self._emitters: Dict[str, RuleEmitter] = {}
        self._register_defaults()
    
    def _register_defaults(self):
        """Register all default rule emitters."""
        numeric_emitter = NumericRuleEmitter()
        for rule_type in [RULE_GT, RULE_GTE, RULE_LT, RULE_LTE, RULE_EQ]:
            self._emitters[rule_type] = numeric_emitter
        
        length_emitter = StringLengthRuleEmitter()
        for rule_type in [RULE_MIN_LEN, RULE_MAX_LEN]:
            self._emitters[rule_type] = length_emitter
        
        format_emitter = StringFormatRuleEmitter()
        for rule_type in [RULE_EMAIL, RULE_HOSTNAME, RULE_IP, RULE_IPV4, RULE_IPV6, RULE_ASCII]:
            self._emitters[rule_type] = format_emitter
        
        pattern_emitter = StringPatternRuleEmitter()
        for rule_type in [RULE_PREFIX, RULE_SUFFIX, RULE_CONTAINS]:
            self._emitters[rule_type] = pattern_emitter
        
        repeated_emitter = RepeatedRuleEmitter()
        for rule_type in [RULE_MIN_ITEMS, RULE_MAX_ITEMS, RULE_UNIQUE]:
            self._emitters[rule_type] = repeated_emitter
        
        # Items emitter for per-item validation on repeated fields
        self._emitters[RULE_ITEMS] = ItemsRuleEmitter()
        
        timestamp_emitter = TimestampRuleEmitter()
        for rule_type in [RULE_TIMESTAMP_GT_NOW, RULE_TIMESTAMP_LT_NOW, RULE_TIMESTAMP_WITHIN]:
            self._emitters[rule_type] = timestamp_emitter
        
        any_emitter = AnyRuleEmitter()
        for rule_type in [RULE_ANY_IN, RULE_ANY_NOT_IN]:
            self._emitters[rule_type] = any_emitter
        
        # Additional emitters
        self._emitters[RULE_REQUIRED] = RequiredRuleEmitter()
        
        in_not_in_emitter = InNotInRuleEmitter()
        for rule_type in [RULE_IN, RULE_NOT_IN]:
            self._emitters[rule_type] = in_not_in_emitter
        
        self._emitters[RULE_ENUM_DEFINED] = EnumDefinedRuleEmitter()
    
    def register(self, rule_type: str, emitter: RuleEmitter):
        """Register an emitter for a rule type."""
        self._emitters[rule_type] = emitter
    
    def _wrap_optional(self, rule_ir: RuleIR, code: str) -> str:
        """
        Wrap code in optional field check if the field is optional.
        
        For optional fields, we only run validation if the has_<field> flag is set.
        """
        if not rule_ir.context.is_optional:
            return code
        
        field_name = rule_ir.field_name
        indented = ''.join(('    ' + line) if line.strip() else line 
                          for line in code.splitlines(True))
        return '        if (msg->has_%s) {\n%s        }\n' % (field_name, indented)
    
    def emit(self, rule_ir: RuleIR, proto_file: Any) -> str:
        """
        Emit C code for a rule using the appropriate emitter.
        
        Args:
            rule_ir: The RuleIR to emit
            proto_file: The ProtoFile for context
            
        Returns:
            C code string, or a TODO comment if no emitter is registered
        """
        emitter = self._emitters.get(rule_ir.rule_type)
        if emitter:
            code = emitter.emit(rule_ir, proto_file)
            # Wrap in optional check if field is optional
            return self._wrap_optional(rule_ir, code)
        return '        /* TODO: Implement rule type %s */\n' % rule_ir.rule_type


# Global emitter registry instance
_emitter_registry = RuleEmitterRegistry()


class ValidatorGenerator:
    """
    Generates C validation code for protobuf messages.
    
    This is the main code generator class. It takes parsed validation rules
    and produces C header and source files containing validation functions.
    
    The generator supports two modes:
    - Normal mode: Validation stops at the first violation
    - Bypass mode: All violations are collected before returning
    
    Attributes:
        proto_file: The ProtoFile object being processed
        validators: OrderedDict mapping message names to MessageValidator objects
        validate_enabled: Whether validation is enabled for this proto file
    """
    
    # =========================================================================
    # Class Constants: Rule Type to C Enum Mappings
    # =========================================================================
    # These mappings centralize the conversion from Python rule types to C macro names.
    # By defining them as class constants, we ensure consistency across all code paths.
    
    NUMERIC_RULE_TO_C_ENUM = {
        RULE_GT: 'PB_VALIDATE_RULE_GT',
        RULE_GTE: 'PB_VALIDATE_RULE_GTE',
        RULE_LT: 'PB_VALIDATE_RULE_LT',
        RULE_LTE: 'PB_VALIDATE_RULE_LTE',
        RULE_EQ: 'PB_VALIDATE_RULE_EQ',
    }
    
    STRING_FORMAT_RULE_TO_C_ENUM = {
        RULE_EMAIL: 'PB_VALIDATE_RULE_EMAIL',
        RULE_HOSTNAME: 'PB_VALIDATE_RULE_HOSTNAME',
        RULE_IP: 'PB_VALIDATE_RULE_IP',
        RULE_IPV4: 'PB_VALIDATE_RULE_IPV4',
        RULE_IPV6: 'PB_VALIDATE_RULE_IPV6',
        RULE_ASCII: 'PB_VALIDATE_RULE_ASCII',
        RULE_PREFIX: 'PB_VALIDATE_RULE_PREFIX',
        RULE_SUFFIX: 'PB_VALIDATE_RULE_SUFFIX',
        RULE_CONTAINS: 'PB_VALIDATE_RULE_CONTAINS',
    }
    
    # Macros for callback string validation
    # Macros for normal string validation
    NORMAL_STRING_FORMAT_MACROS = {
        RULE_EMAIL: 'PB_VALIDATE_STR_EMAIL',
        RULE_HOSTNAME: 'PB_VALIDATE_STR_HOSTNAME',
        RULE_IP: 'PB_VALIDATE_STR_IP',
        RULE_IPV4: 'PB_VALIDATE_STR_IPV4',
        RULE_IPV6: 'PB_VALIDATE_STR_IPV6',
    }
    
    def __init__(self, proto_file: Any):
        """
        Initialize the ValidatorGenerator.
        
        Args:
            proto_file: The ProtoFile object containing messages to validate
        """
        self.proto_file = proto_file
        self.validators = OrderedDict()
        
        # Pipeline components
        self.ir_builder = IRBuilder(proto_file)
        self.emitter_registry = _emitter_registry
        
        # Check if validation is enabled in file options
        self.validate_enabled = False
        if hasattr(proto_file, 'file_options') and hasattr(proto_file.file_options, 'validate'):
            self.validate_enabled = proto_file.file_options.validate
    
    def add_message_validator(self, message: Any, message_rules: Optional[Any] = None) -> None:
        """
        Add a validator for a message (only if it has validation rules).
        
        Args:
            message: The protobuf message descriptor
            message_rules: The MessageRules option from validate.proto (or None)
        """
        validator = MessageValidator(message, message_rules, self.proto_file)
        if validator.field_validators or validator.message_rules or validator.oneof_validators:
            self.validators[str(message.name)] = validator
    
    def force_add_message_validator(self, message: Any, message_rules: Optional[Any] = None) -> None:
        """
        Force add a validator for a message, even if no rules are present.
        
        This is used when a message has no rules of its own but still needs a
        validator function generated for it.
        We still generate a validation function that always returns true.
        
        Args:
            message: The protobuf message descriptor
            message_rules: The MessageRules option from validate.proto (or None)
        """
        validator = MessageValidator(message, message_rules, self.proto_file)
        self.validators[str(message.name)] = validator
    
    # =========================================================================
    # Documentation and Display Helpers
    # =========================================================================

    def _rule_to_text(self, rule: ValidationRule) -> str:
        """Convert a single ValidationRule into a human-friendly sentence fragment."""
        rt = rule.rule_type
        p = rule.params or {}
        val = p.get('value')
        values = p.get('values')
        def list_render(vals):
            try:
                return '{' + ', '.join(str(v) for v in vals) + '}'
            except Exception:
                return '{...}'
        if rt == RULE_REQUIRED:
            return 'required'
        if rt == RULE_ONEOF_REQUIRED:
            return 'one-of group requires a value'
        if rt == RULE_EQ:
            return f'== {val}'
        if rt == RULE_GT:
            return f'> {val}'
        if rt == RULE_GTE:
            return f'>= {val}'
        if rt == RULE_LT:
            return f'< {val}'
        if rt == RULE_LTE:
            return f'<= {val}'
        if rt == RULE_IN:
            return f'in {list_render(values)}'
        if rt == RULE_NOT_IN:
            return f'not in {list_render(values)}'
        if rt == RULE_MIN_LEN:
            return f'min length {val}'
        if rt == RULE_MAX_LEN:
            return f'max length {val}'
        if rt == RULE_PREFIX:
            return f'prefix "{_escape_for_comment(val)}"'
        if rt == RULE_SUFFIX:
            return f'suffix "{_escape_for_comment(val)}"'
        if rt == RULE_CONTAINS:
            return f'contains "{_escape_for_comment(val)}"'
        if rt == RULE_ASCII:
            return 'ASCII only'
        if rt == RULE_EMAIL:
            return 'valid email address'
        if rt == RULE_HOSTNAME:
            return 'valid hostname'
        if rt == RULE_IP:
            return 'valid IP address'
        if rt == RULE_IPV4:
            return 'valid IPv4 address'
        if rt == RULE_IPV6:
            return 'valid IPv6 address'
        if rt == RULE_ENUM_DEFINED:
            return 'must be a defined enum value'
        if rt == RULE_MIN_ITEMS:
            return f'at least {val} items'
        if rt == RULE_MAX_ITEMS:
            return f'at most {val} items'
        if rt == RULE_UNIQUE:
            return 'items must be unique'
        if rt == RULE_ITEMS:
            return 'per-item validation rules'
        if rt == RULE_NO_SPARSE:
            return 'no sparse map entries'
        # Fallback
        return rt.replace('_', ' ').lower()

    def _format_field_constraints(self, field_name: str, fv: 'FieldValidator') -> str:
        """Produce a concise, pretty description line for a field and its constraints."""
        try:
            # Group multiple fragments with semicolons
            parts = [self._rule_to_text(r) for r in fv.rules]
            # Deduplicate while preserving order
            seen = set()
            uniq = []
            for t in parts:
                if t not in seen:
                    seen.add(t)
                    uniq.append(t)
            if not uniq:
                return f"- {field_name}: no constraints"
            return f"- {field_name}: " + '; '.join(uniq)
        except Exception:
            return f"- {field_name}: (constraints unavailable)"
    # ----------------------------------------------------------------------

    def _collect_validation_dependencies(self):
        """
        Collect validation header dependencies from imported proto files.
        
        Returns a set of validation header paths that need to be included.
        These are proto files that define messages used as field types in this file.
        """
        dep_headers = set()
        
        # Iterate through all validators and their messages
        for validator in self.validators.values():
            message = validator.message
            # Check all fields in the message
            for field in getattr(message, 'fields', []):
                try:
                    # Only consider message-type fields
                    if getattr(field, 'pbtype', None) not in ('MESSAGE', 'MSG_W_CB'):
                        continue
                    
                    # Get the message type
                    submsg_ctype = getattr(field, 'ctype', None)
                    if not submsg_ctype:
                        continue
                    
                    # Skip google.protobuf.Any - it doesn't have validation
                    submsg_ctype_str = str(submsg_ctype).lower()
                    if 'google' in submsg_ctype_str and 'protobuf' in submsg_ctype_str:
                        continue
                    
                    # Look up the message in dependencies
                    dep_msg = self.proto_file.dependencies.get(str(submsg_ctype))
                    if dep_msg and hasattr(dep_msg, 'protofile'):
                        dep_protofile = dep_msg.protofile
                        # Only add if it's from a different proto file
                        if dep_protofile != self.proto_file:
                            dep_name = dep_protofile.fdesc.name
                            if dep_name.endswith('.proto'):
                                dep_name = dep_name[:-6]
                            dep_headers.add(dep_name)
                except Exception:
                    # Skip fields with unexpected structure
                    pass
        
        return sorted(dep_headers)

    def generate_header(self):
        """Generate validation header file content."""
        if not self.validators:
            return
        
        # Extract base name without .proto extension
        base_name = self.proto_file.fdesc.name
        if base_name.endswith('.proto'):
            base_name = base_name[:-6]
        
        guard = 'PB_VALIDATE_%s_INCLUDED' % base_name.upper().replace('.', '_').replace('/', '_').replace('-', '_')
        
        yield '/* Validation header for %s */\n' % self.proto_file.fdesc.name
        yield '#ifndef %s\n' % guard
        yield '#define %s\n' % guard
        yield '\n'
        yield '#include <pb_validate.h>\n'
        yield '#include "%s.pb.h"\n' % base_name
        
        # Include validation headers for imported proto files
        dep_headers = self._collect_validation_dependencies()
        for dep_header in dep_headers:
            yield '#include "%s_validate.h"\n' % dep_header
        
        yield '\n'
        yield '#ifdef __cplusplus\n'
        yield 'extern "C" {\n'
        yield '#endif\n'
        yield '\n'
        
        # Generate validation function declarations with rich Doxygen
        for msg_name, validator in self.validators.items():
            # Use the C struct name from the message
            struct_name = str(validator.message.name)
            func_name = 'pb_validate_' + struct_name.replace('.', '_')
            # Build field description list
            try:
                all_field_names = []
                for f in getattr(validator.message, 'fields', []) or []:
                    fname = getattr(f, 'name', None)
                    if fname:
                        all_field_names.append(fname)
                validated_field_names = list(validator.field_validators.keys())
                fields_without = [fn for fn in all_field_names if fn not in validated_field_names]
            except Exception:
                all_field_names, fields_without = [], []

            # Doxygen header
            yield '/**\n'
            yield ' * @brief Validate %s message.\n' % struct_name
            yield ' *\n'
            yield ' * Fields and constraints:\n'
            # With constraints
            for fname, fv in validator.field_validators.items():
                desc = _escape_for_comment(self._format_field_constraints(fname, fv))
                yield ' * %s\n' % desc
            # Oneof validators
            for oneof_name, oneof_data in validator.oneof_validators.items():
                yield ' * - oneof %s:\n' % _escape_for_comment(oneof_name)
                for member_field, member_fv in oneof_data['members']:
                    desc = _escape_for_comment(self._format_field_constraints(member_field.name, member_fv))
                    yield ' *   %s\n' % desc
            # Without constraints
            for fname in fields_without:
                # Skip oneof names as they are already documented above
                if fname in validator.oneof_validators:
                    continue
                yield ' * - %s: no constraints\n' % _escape_for_comment(fname)
            # Message-level rules summary
            if validator.message_rules:
                yield ' *\n'
                yield ' * Message-level rules:\n'
                for mr in validator.message_rules:
                    try:
                        if mr.rule_type == 'REQUIRES':
                            yield ' * - requires field "%s"\n' % _escape_for_comment(mr.params.get('field', ''))
                        elif mr.rule_type == 'MUTEX':
                            fields = mr.params.get('fields', [])
                            yield ' * - mutual exclusion among %s\n' % _escape_for_comment('{'+', '.join(fields)+'}')
                        elif mr.rule_type == 'AT_LEAST':
                            n = mr.params.get('n', 1)
                            fields = mr.params.get('fields', [])
                            yield ' * - at least %s of %s must be set\n' % (str(n), _escape_for_comment('{'+', '.join(fields)+'}'))
                        else:
                            yield ' * - %s\n' % _escape_for_comment(mr.rule_type.lower())
                    except Exception:
                        yield ' * - (message rule)\n'
            yield ' *\n'
            yield ' * @param msg [in] Pointer to %s instance to validate.\n' % struct_name
            yield ' * @param violations [out] Violations accumulator for collecting errors.\n'
            
            yield ' * @return true if valid, false otherwise.\n'
            yield ' */\n'
            
            yield 'bool %s(const %s *msg, pb_violations_t *violations);\n' % (func_name, struct_name)
            
            yield '\n'
        
        yield '#ifdef __cplusplus\n'
        yield '} /* extern "C" */\n'
        yield '#endif\n'
        yield '\n'
        yield '#endif /* %s */\n' % guard
    
    # =========================================================================
    # IR-Based Source Generation (AUTHORITATIVE PIPELINE)
    # =========================================================================
    # All code generation routes through the IR pipeline:
    #   ValidationRule → IRBuilder → RuleIR → RuleEmitter → C code
    
    def _emit_rule(self, rule: ValidationRule, field: Any, 
                   is_oneof: bool = False, oneof_name: str = '', 
                   is_anonymous: bool = False) -> str:
        """
        Emit C code for a single validation rule using the IR pipeline.
        
        This is the SINGLE authoritative code generation path. All rule emission
        must go through this method.
        
        Args:
            rule: The ValidationRule to emit
            field: The field descriptor
            is_oneof: Whether this field is in a oneof group
            oneof_name: Name of the oneof group (if applicable)
            is_anonymous: Whether the oneof is anonymous
            
        Returns:
            C code string for this rule
        """
        # Build field context
        if is_oneof:
            context = FieldContext.for_oneof_member(field, oneof_name, is_anonymous)
        else:
            context = FieldContext.for_regular_field(field)
        
        # Build RuleIR
        rule_ir = self.ir_builder.build_rule_ir(rule, context)
        
        # Emit via emitter registry
        return self.emitter_registry.emit(rule_ir, self.proto_file)
    
    def _emit_field_rules_from_ir(self, field_rule_set: FieldRuleSet) -> str:
        """
        Emit C code for all rules in a FieldRuleSet using the IR pipeline.
        
        Args:
            field_rule_set: The FieldRuleSet containing all rules for a field
            
        Returns:
            C code string for validating this field
        """
        code_parts = []
        
        for rule_ir in field_rule_set.rules:
            code = self.emitter_registry.emit(rule_ir, self.proto_file)
            if code:
                code_parts.append(code)
        
        return ''.join(code_parts)
    
    def _emit_message_validation_from_ir(self, msg_rule_set: MessageRuleSet) -> str:
        """
        Emit the complete validation function body for a message using IR.
        
        This method demonstrates the pipeline-driven approach:
        1. Iterate over FieldRuleSets and emit field validations
        2. Iterate over OneofRuleSets and emit oneof validations
        3. Emit message-level validations
        
        Args:
            msg_rule_set: The complete MessageRuleSet for the message
            
        Returns:
            C code string for the validation function body
        """
        lines = []
        
        # Emit field validations
        for frs in msg_rule_set.field_rule_sets:
            if frs.has_rules():
                lines.append('    /* Validate field: %s */\n' % frs.field_name)
                lines.append('    PB_VALIDATE_FIELD_BEGIN(ctx, "%s");\n' % frs.field_name)
                lines.append(self._emit_field_rules_from_ir(frs))
                lines.append('    PB_VALIDATE_FIELD_END(ctx);\n')
                lines.append('\n')
        
        # Emit oneof validations
        for ors in msg_rule_set.oneof_rule_sets:
            lines.append('    /* Validate oneof: %s */\n' % ors.oneof_name)
            lines.append('    PB_VALIDATE_ONEOF_BEGIN(ctx, msg, %s)\n' % ors.oneof_name)
            
            for member_frs in ors.member_rule_sets:
                tag_name = '%s_%s_tag' % (msg_rule_set.struct_name, member_frs.field_name)
                lines.append('    PB_VALIDATE_ONEOF_CASE(%s)\n' % tag_name)
                lines.append('    PB_VALIDATE_FIELD_BEGIN(ctx, "%s");\n' % member_frs.field_name)
                lines.append(self._emit_field_rules_from_ir(member_frs))
                lines.append('    PB_VALIDATE_FIELD_END(ctx);\n')
                lines.append('    PB_VALIDATE_ONEOF_CASE_END()\n')
            
            lines.append('    PB_VALIDATE_ONEOF_DEFAULT()\n')
            lines.append('    PB_VALIDATE_ONEOF_END()\n')
            lines.append('\n')
        
        return ''.join(lines)

    def generate_source(self):
        """Generate validation source file content."""
        if not self.validators:
            return
        # Extract base name without .proto extension
        base_name = self.proto_file.fdesc.name
        if base_name.endswith('.proto'):
            base_name = base_name[:-6]

        yield '/* Validation implementation for %s */\n' % self.proto_file.fdesc.name
        yield '#include "%s_validate.h"\n' % base_name
        yield '#include <string.h>\n'
        yield '\n'
        
        # Generate static rule data
        for msg_name, validator in self.validators.items():
            # Use the C struct name from the message
            struct_name = str(validator.message.name)
            func_name = 'pb_validate_' + struct_name.replace('.', '_')
            
            # Generate comment listing fields without constraints
            # Exclude: validated fields, oneof names, oneof member fields, and nested message fields
            all_field_names = set()
            nested_message_fields = set()
            for f in getattr(validator.message, 'fields', []):
                try:
                    field_name = getattr(f, 'name', None)
                    if field_name:
                        all_field_names.add(field_name)
                        # Track nested message fields
                        if getattr(f, 'pbtype', None) in ('MESSAGE', 'MSG_W_CB'):
                            nested_message_fields.add(field_name)
                except Exception:
                    pass
            
            # Get validated field names (direct field validators)
            validated_field_names = set(validator.field_validators.keys())
            
            # Get oneof names and their member field names
            oneof_names = set(validator.oneof_validators.keys())
            oneof_member_names = set()
            for oneof_name, oneof_data in validator.oneof_validators.items():
                for member_field, member_fv in oneof_data['members']:
                    oneof_member_names.add(member_field.name)
            
            # Exclude validated fields, oneofs, oneof members, and nested message fields
            fields_without_constraints = sorted(
                all_field_names - validated_field_names - oneof_names - oneof_member_names - nested_message_fields
            )

            # Generate validation function
            yield 'bool %s(const %s *msg, pb_violations_t *violations)\n' % (func_name, struct_name)
            
            yield '{\n'
            if fields_without_constraints:
                    yield '    /* Fields without constraints:\n'
                    for field in fields_without_constraints:
                        yield '       - %s\n' % field
                    yield '    */\n'
                    yield '\n'
            yield '    PB_VALIDATE_BEGIN(ctx, %s, msg, violations);\n' % struct_name
            yield '\n'
            
            # Generate field validations
            for field_name, field_validator in validator.field_validators.items():
                field = field_validator.field
                field_var_name = Globals.naming_style.var_name(field_name)
                
                yield '    /* Validate field: %s */\n' % field_name
                yield '    PB_VALIDATE_FIELD_BEGIN(ctx, "%s");\n' % field_name
                
                for rule in field_validator.rules:
                    yield self._emit_rule(rule, field)

                # Automatic recursion for nested message fields
                # When a field contains another message, we need to recursively
                # validate that nested message as well
                try:
                    is_message_field = getattr(field, 'pbtype', None) in ('MESSAGE', 'MSG_W_CB')
                    submsg_ctype = getattr(field, 'ctype', None)
                    if is_message_field and submsg_ctype:
                        # Skip google.protobuf.Any and google.protobuf.Timestamp - they have special validation
                        submsg_ctype_str = str(submsg_ctype).lower()
                        is_google_special = ('google' in submsg_ctype_str and 
                                            'protobuf' in submsg_ctype_str and 
                                            ('any' in submsg_ctype_str or 'timestamp' in submsg_ctype_str))
                        if not is_google_special:
                            allocation = getattr(field, 'allocation', None)
                            # Generate the nested validation function name
                            sub_func = 'pb_validate_' + str(submsg_ctype).replace('.', '_')
                            rules = getattr(field, 'rules', None)
                                
                            # Use different macros based on allocation type
                            if allocation == 'POINTER':
                                yield '    PB_VALIDATE_NESTED_MSG_POINTER(ctx, %s, msg, %s, violations);\n' % (sub_func, field_name)
                            else:
                                if rules == 'OPTIONAL':
                                    yield '    PB_VALIDATE_NESTED_MSG_OPTIONAL(ctx, %s, msg, %s, violations);\n' % (sub_func, field_name)
                                else:
                                    yield '    PB_VALIDATE_NESTED_MSG(ctx, %s, msg, %s, violations);\n' % (sub_func, field_name)
                except Exception:
                    # If field shape is unexpected, skip recursion silently to avoid crashes
                    pass
                
                yield '    PB_VALIDATE_FIELD_END(ctx);\n'
                yield '\n'
            
            # Also recurse into message-typed fields that have no field-level rules
            try:
                field_names_with_rules = set(validator.field_validators.keys())
            except Exception:
                field_names_with_rules = set()
            for f in getattr(validator.message, 'fields', []):
                try:
                    if getattr(f, 'name', None) in field_names_with_rules:
                        continue
                    if getattr(f, 'pbtype', None) not in ('MESSAGE', 'MSG_W_CB'):
                        continue
                    fname = getattr(f, 'name')
                    submsg_ctype = getattr(f, 'ctype', None)
                    if not submsg_ctype:
                        continue
                    # Skip google.protobuf.Any and google.protobuf.Timestamp - they have special validation
                    submsg_ctype_str = str(submsg_ctype).lower()
                    if 'google' in submsg_ctype_str and 'protobuf' in submsg_ctype_str and ('any' in submsg_ctype_str or 'timestamp' in submsg_ctype_str):
                        continue
                    
                    allocation = getattr(f, 'allocation', None)
                    
                    sub_func = 'pb_validate_' + str(submsg_ctype).replace('.', '_')
                    rules = getattr(f, 'rules', None)
                    # Open path context
                    yield '    /* Validate field: %s */\n' % fname
                    yield '    PB_VALIDATE_FIELD_BEGIN(ctx, "%s");\n' % fname
                    if allocation == 'POINTER':
                        yield '    PB_VALIDATE_NESTED_MSG_POINTER(ctx, %s, msg, %s, violations);\n' % (sub_func, fname)
                    else:
                        if rules == 'OPTIONAL':
                            yield '    PB_VALIDATE_NESTED_MSG_OPTIONAL(ctx, %s, msg, %s, violations);\n' % (sub_func, fname)
                        else:
                            yield '    PB_VALIDATE_NESTED_MSG(ctx, %s, msg, %s, violations);\n' % (sub_func, fname)
                    yield '    PB_VALIDATE_FIELD_END(ctx);\n'
                    yield '\n'
                except Exception:
                    # Skip if field metadata is unexpected
                    pass

            # Generate oneof member validations (switch-based)
            for oneof_name, oneof_data in validator.oneof_validators.items():
                oneof_obj = oneof_data['oneof']
                members = oneof_data['members']  # List of (field, FieldValidator) tuples
                
                yield '    /* Validate oneof: %s */\n' % oneof_name
                yield '    PB_VALIDATE_ONEOF_BEGIN(ctx, msg, %s)\n' % oneof_name
                
                for member_field, member_fv in members:
                    field_name = member_field.name
                    # Generate case for this oneof member
                    # Tag constant format: <MESSAGE_NAME>_<field_name>_tag
                    tag_name = '%s_%s_tag' % (struct_name, field_name)
                    yield '    PB_VALIDATE_ONEOF_CASE(%s)\n' % tag_name
                    yield '    PB_VALIDATE_FIELD_BEGIN(ctx, "%s");\n' % field_name
                    
                    # Get anonymous flag for the oneof
                    is_anonymous = getattr(oneof_obj, 'anonymous', False)
                    
                    for rule in member_fv.rules:
                        yield self._emit_rule(rule, member_field, is_oneof=True, 
                                             oneof_name=oneof_name, is_anonymous=is_anonymous)
                    
                    yield '    PB_VALIDATE_FIELD_END(ctx);\n'
                    yield '    PB_VALIDATE_ONEOF_CASE_END()\n'
                
                yield '    PB_VALIDATE_ONEOF_DEFAULT()\n'
                yield '    PB_VALIDATE_ONEOF_END()\n'
                yield '\n'

            # Generate message-level validations
            for rule in validator.message_rules:
                yield '    /* Message rule: %s */\n' % rule.constraint_id
                yield self._generate_message_rule_check(validator.message, rule)
                yield '\n'
            
            yield '    PB_VALIDATE_END(ctx, violations);\n'
            yield '}\n'
            yield '\n'

    
    def _escape_c_string(self, s: str) -> str:
        """
        Escape a string for use in C code.
        
        Handles all standard C escape sequences to ensure safe embedding
        of arbitrary string values in generated C code.
        """
        result = s.replace('\\', '\\\\')  # Must be first to avoid double-escaping
        result = result.replace('"', '\\"')
        result = result.replace('\n', '\\n')
        result = result.replace('\r', '\\r')
        result = result.replace('\t', '\\t')
        result = result.replace('\0', '\\0')
        result = result.replace('\v', '\\v')
        result = result.replace('\f', '\\f')
        result = result.replace('\b', '\\b')
        result = result.replace('\a', '\\a')
        return result
    # =========================================================================
    # Message-Level Rule Code Generators
    # =========================================================================
    # These methods generate C code for message-level validation rules
    # (requires, mutex, at_least constraints).

    def _generate_message_rule_check(self, message: Any, rule: ValidationRule) -> str:
        """
        Generate code for a message-level validation rule using the IR pipeline.
        
        Args:
            message: The message descriptor
            rule: The ValidationRule to generate code for
            
        Returns:
            C code string implementing the rule check
        """
        # Build IR for message rule and emit via the pipeline
        context = FieldContext.for_message_rule()
        rule_ir = self.ir_builder.build_rule_ir(rule, context)
        result = self.emitter_registry.emit(rule_ir, self.proto_file)
        if result:
            return result
        # Fallback for unimplemented message rule types
        return '    /* TODO: Implement message rule type %s */\n' % rule.rule_type
