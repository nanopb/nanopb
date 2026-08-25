#!/usr/bin/env python3
# kate: replace-tabs on; indent-width 4;

"""
nanopb test data generator.

This module generates valid and invalid protobuf test data based on
validation constraints defined in .proto files using validate.proto.
"""

import os
import sys
import random
import string
import importlib
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple, Union

# Prefer pure-Python protobuf runtime to support loading generated *_pb2
# files built with older protoc versions (avoids descriptor runtime errors).
os.environ.setdefault('PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION', 'python')

try:
    from google.protobuf import any_pb2
    from google.protobuf import descriptor_pb2
    from google.protobuf import descriptor_pool
    from google.protobuf import message_factory
    from google.protobuf.descriptor import Descriptor as RuntimeDescriptor
    from google.protobuf.descriptor import FieldDescriptor as RuntimeFieldDescriptor
    from google.protobuf.message import Message
except ImportError:
    sys.stderr.write("Error: protobuf library required. Install with: pip install protobuf\n")
    sys.exit(1)

try:
    from .proto._utils import invoke_protoc
    from .proto import TemporaryDirectory
except ImportError:
    from proto._utils import invoke_protoc
    from proto import TemporaryDirectory


def _load_validate_pb2() -> Any:
    """Load validate_pb2 from the repository-local generator/proto directory."""

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if repo_root not in sys.path:
        sys.path.insert(0, repo_root)
    from generator.proto import validate_pb2
    return validate_pb2


class OutputFormat(Enum):
    """Output format for generated data."""

    BINARY = "binary"
    C_ARRAY = "c_array"
    PYTHON_DICT = "python_dict"
    HEX_STRING = "hex_string"


@dataclass
class ValidationConstraint:
    """Represents a validation constraint for a field."""

    field_name: str
    field_type: str
    rule_type: str
    value: Any


@dataclass
class ProtoFieldInfo:
    """Information about a protobuf field."""

    descriptor: descriptor_pb2.FieldDescriptorProto
    name: str = field(init=False)
    number: int = field(init=False)
    type: int = field(init=False)
    type_name: str = field(init=False)
    label: int = field(init=False)
    constraints: List[ValidationConstraint] = field(default_factory=list, init=False)

    _TYPE_MAP = {
        descriptor_pb2.FieldDescriptorProto.TYPE_DOUBLE: 'double',
        descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT: 'float',
        descriptor_pb2.FieldDescriptorProto.TYPE_INT64: 'int64',
        descriptor_pb2.FieldDescriptorProto.TYPE_UINT64: 'uint64',
        descriptor_pb2.FieldDescriptorProto.TYPE_INT32: 'int32',
        descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64: 'fixed64',
        descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32: 'fixed32',
        descriptor_pb2.FieldDescriptorProto.TYPE_BOOL: 'bool',
        descriptor_pb2.FieldDescriptorProto.TYPE_STRING: 'string',
        descriptor_pb2.FieldDescriptorProto.TYPE_GROUP: 'group',
        descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE: 'message',
        descriptor_pb2.FieldDescriptorProto.TYPE_BYTES: 'bytes',
        descriptor_pb2.FieldDescriptorProto.TYPE_UINT32: 'uint32',
        descriptor_pb2.FieldDescriptorProto.TYPE_ENUM: 'enum',
        descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED32: 'sfixed32',
        descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED64: 'sfixed64',
        descriptor_pb2.FieldDescriptorProto.TYPE_SINT32: 'sint32',
        descriptor_pb2.FieldDescriptorProto.TYPE_SINT64: 'sint64',
    }

    def __post_init__(self) -> None:
        self.name = self.descriptor.name
        self.number = self.descriptor.number
        self.type = self.descriptor.type
        self.type_name = self.descriptor.type_name
        self.label = self.descriptor.label
        if hasattr(self.descriptor, 'options'):
            self._parse_validation_rules(self.descriptor.options)

    def _parse_validation_rules(self, field_options: Any) -> None:
        """Parse validation rules from field options."""

        try:
            validate_pb2 = _load_validate_pb2()
            if not field_options.HasExtension(validate_pb2.rules):
                return

            rules_option = field_options.Extensions[validate_pb2.rules]
        except Exception:
            return

        def add_constraint(rule_type: str, value: Any) -> None:
            self.constraints.append(
                ValidationConstraint(
                    self.name,
                    self.get_type_name(),
                    rule_type,
                    value,
                )
            )

        def add_numeric_rules(numeric_rules: Any) -> None:
            if numeric_rules.HasField('const_value'):
                add_constraint('const', numeric_rules.const_value)
            if numeric_rules.HasField('lt'):
                add_constraint('lt', numeric_rules.lt)
            if numeric_rules.HasField('lte'):
                add_constraint('lte', numeric_rules.lte)
            if numeric_rules.HasField('gt'):
                add_constraint('gt', numeric_rules.gt)
            if numeric_rules.HasField('gte'):
                add_constraint('gte', numeric_rules.gte)
            if getattr(numeric_rules, 'in'):
                add_constraint('in', list(getattr(numeric_rules, 'in')))
            if getattr(numeric_rules, 'not_in'):
                add_constraint('not_in', list(getattr(numeric_rules, 'not_in')))

        numeric_types = [
            'int32', 'int64', 'uint32', 'uint64',
            'sint32', 'sint64', 'fixed32', 'fixed64',
            'sfixed32', 'sfixed64', 'float', 'double',
        ]
        for type_name in numeric_types:
            if rules_option.HasField(type_name):
                add_numeric_rules(getattr(rules_option, type_name))

        if rules_option.HasField('string'):
            string_rules = rules_option.string
            if string_rules.HasField('const_value'):
                add_constraint('const', string_rules.const_value)
            if string_rules.HasField('min_len'):
                add_constraint('min_len', string_rules.min_len)
            if string_rules.HasField('max_len'):
                add_constraint('max_len', string_rules.max_len)
            if string_rules.HasField('prefix'):
                add_constraint('prefix', string_rules.prefix)
            if string_rules.HasField('suffix'):
                add_constraint('suffix', string_rules.suffix)
            if string_rules.HasField('contains'):
                add_constraint('contains', string_rules.contains)
            if string_rules.HasField('ascii') and string_rules.ascii:
                add_constraint('ascii', True)
            if getattr(string_rules, 'email', False):
                add_constraint('email', True)
            if getattr(string_rules, 'hostname', False):
                add_constraint('hostname', True)
            if getattr(string_rules, 'ip', False):
                add_constraint('ip', True)
            if getattr(string_rules, 'ipv4', False):
                add_constraint('ipv4', True)
            if getattr(string_rules, 'ipv6', False):
                add_constraint('ipv6', True)
            if getattr(string_rules, 'in'):
                add_constraint('in', list(getattr(string_rules, 'in')))
            if getattr(string_rules, 'not_in'):
                add_constraint('not_in', list(getattr(string_rules, 'not_in')))

        if rules_option.HasField('bytes'):
            bytes_rules = rules_option.bytes
            if bytes_rules.HasField('const_value'):
                add_constraint('const', bytes_rules.const_value)
            if bytes_rules.HasField('min_len'):
                add_constraint('min_len', bytes_rules.min_len)
            if bytes_rules.HasField('max_len'):
                add_constraint('max_len', bytes_rules.max_len)
            if bytes_rules.HasField('prefix'):
                add_constraint('prefix', bytes_rules.prefix)
            if bytes_rules.HasField('suffix'):
                add_constraint('suffix', bytes_rules.suffix)
            if bytes_rules.HasField('contains'):
                add_constraint('contains', bytes_rules.contains)
            if getattr(bytes_rules, 'in'):
                add_constraint('in', list(getattr(bytes_rules, 'in')))
            if getattr(bytes_rules, 'not_in'):
                add_constraint('not_in', list(getattr(bytes_rules, 'not_in')))

        if rules_option.HasField('bool') and rules_option.bool.HasField('const_value'):
            add_constraint('const', rules_option.bool.const_value)

        if rules_option.HasField('enum'):
            enum_rules = rules_option.enum
            if enum_rules.HasField('const_value'):
                add_constraint('const', enum_rules.const_value)
            if getattr(enum_rules, 'in'):
                add_constraint('in', list(getattr(enum_rules, 'in')))
            if getattr(enum_rules, 'not_in'):
                add_constraint('not_in', list(getattr(enum_rules, 'not_in')))

        if rules_option.HasField('repeated'):
            repeated_rules = rules_option.repeated
            if repeated_rules.HasField('min_items'):
                add_constraint('min_items', repeated_rules.min_items)
            if repeated_rules.HasField('max_items'):
                add_constraint('max_items', repeated_rules.max_items)
            if repeated_rules.HasField('unique') and repeated_rules.unique:
                add_constraint('unique', True)

        if rules_option.HasField('map'):
            map_rules = rules_option.map
            if map_rules.HasField('min_pairs'):
                add_constraint('min_items', map_rules.min_pairs)
            if map_rules.HasField('max_pairs'):
                add_constraint('max_items', map_rules.max_pairs)

        if rules_option.HasField('any'):
            any_rules = rules_option.any
            if getattr(any_rules, 'in'):
                add_constraint('in', list(getattr(any_rules, 'in')))
            if getattr(any_rules, 'not_in'):
                add_constraint('not_in', list(getattr(any_rules, 'not_in')))

        if rules_option.HasField('required') and rules_option.required:
            self.constraints.append(
                ValidationConstraint(self.name, self.get_type_name(), 'required', True)
            )
        if rules_option.HasField('oneof_required') and rules_option.oneof_required:
            self.constraints.append(
                ValidationConstraint(self.name, self.get_type_name(), 'oneof_required', True)
            )

    def get_type_name(self) -> str:
        """Get human-readable type name."""

        return self._TYPE_MAP.get(self.type, 'unknown')

    def full_type_name(self) -> str:
        """Get the fully qualified message or enum type name."""

        return self.type_name.lstrip('.') if self.type_name else ''

    def is_repeated(self) -> bool:
        """Check if field is repeated."""

        return self.label == descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED

    def is_message(self) -> bool:
        """Check if field is a message field."""

        return self.type == descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE

    def is_any(self) -> bool:
        """Check if field is google.protobuf.Any."""

        return self.full_type_name() == 'google.protobuf.Any'

    def constraint_map(self) -> Dict[str, Any]:
        """Get constraints as a rule->value mapping."""

        return {constraint.rule_type: constraint.value for constraint in self.constraints}


@dataclass
class MessageInfo:
    """Descriptor metadata for a message type."""

    full_name: str
    short_name: str
    descriptor_proto: descriptor_pb2.DescriptorProto
    file_descriptor: descriptor_pb2.FileDescriptorProto
    fields: Dict[str, ProtoFieldInfo]


@dataclass(frozen=True)
class AnyCandidate:
    """Concrete message candidate for an Any field."""

    message_info: MessageInfo
    type_url: str


class DescriptorRegistry:
    """Registry for proto descriptors and dynamic message resolution."""

    def __init__(
        self,
        file_set: descriptor_pb2.FileDescriptorSet,
        target_file: descriptor_pb2.FileDescriptorProto,
    ):
        self.file_set = file_set
        self.target_file = target_file
        self.pool = descriptor_pool.DescriptorPool()
        self._factory = None
        self.messages_by_full_name: Dict[str, MessageInfo] = {}
        self.target_top_level_names: List[str] = []
        self._top_level_aliases: Dict[str, str] = {}
        self._build()

    def _build(self) -> None:
        self._add_files_to_pool()
        for file_desc in self.file_set.file:
            self._register_file_messages(file_desc)

    def _add_files_to_pool(self) -> None:
        pending = {file_desc.name: file_desc for file_desc in self.file_set.file}
        added = set()

        while pending:
            progressed = False
            for name, file_desc in list(pending.items()):
                if any(dep not in added and dep in pending for dep in file_desc.dependency):
                    continue
                self.pool.AddSerializedFile(file_desc.SerializeToString())
                added.add(name)
                del pending[name]
                progressed = True

            if progressed:
                continue

            name, file_desc = next(iter(pending.items()))
            self.pool.AddSerializedFile(file_desc.SerializeToString())
            added.add(name)
            del pending[name]

    def _register_file_messages(self, file_desc: descriptor_pb2.FileDescriptorProto) -> None:
        package_prefix = file_desc.package
        for message_desc in file_desc.message_type:
            self._register_message(file_desc, message_desc, package_prefix, top_level=True)

    def _register_message(
        self,
        file_desc: descriptor_pb2.FileDescriptorProto,
        message_desc: descriptor_pb2.DescriptorProto,
        prefix: str,
        top_level: bool = False,
    ) -> None:
        full_name = '.'.join(part for part in [prefix, message_desc.name] if part)
        fields = {
            field_desc.name: ProtoFieldInfo(field_desc)
            for field_desc in message_desc.field
        }
        self.messages_by_full_name[full_name] = MessageInfo(
            full_name=full_name,
            short_name=message_desc.name,
            descriptor_proto=message_desc,
            file_descriptor=file_desc,
            fields=fields,
        )

        if top_level and file_desc.name == self.target_file.name:
            self.target_top_level_names.append(message_desc.name)
            self._top_level_aliases[message_desc.name] = full_name

        nested_prefix = full_name
        for nested_desc in message_desc.nested_type:
            self._register_message(file_desc, nested_desc, nested_prefix)

    def resolve_message(self, message_name: str) -> MessageInfo:
        """Resolve a message by full name or target-file top-level alias."""

        normalized = message_name.lstrip('.')
        if normalized in self.messages_by_full_name:
            return self.messages_by_full_name[normalized]
        if normalized in self._top_level_aliases:
            return self.messages_by_full_name[self._top_level_aliases[normalized]]
        raise ValueError(f"Message {message_name} not found")

    def get_runtime_descriptor(self, message_name: str) -> RuntimeDescriptor:
        """Get runtime descriptor for a message."""

        full_name = self.resolve_message(message_name).full_name
        return self.pool.FindMessageTypeByName(full_name)

    def get_message_class(self, message_name: str):
        """Get a dynamic protobuf message class."""

        descriptor = self.get_runtime_descriptor(message_name)
        return self.get_message_class_for_descriptor(descriptor)

    def get_message_class_for_descriptor(self, descriptor: RuntimeDescriptor):
        """Get a dynamic protobuf message class from a runtime descriptor."""

        try:
            return message_factory.GetMessageClass(descriptor)
        except AttributeError:
            if self._factory is None:
                self._factory = message_factory.MessageFactory(self.pool)
            return self._factory.GetPrototype(descriptor)

    def iter_messages(self) -> Iterable[MessageInfo]:
        """Iterate over all indexed messages."""

        return self.messages_by_full_name.values()


class DataGenerator:
    """Generates test data for protobuf messages with validation rules."""

    MAX_MESSAGE_DEPTH = 8
    _SYSTEM_ANY_EXCLUDED_FILES = {
        'validate.proto',
        'nanopb.proto',
        'google/protobuf/any.proto',
        'google/protobuf/descriptor.proto',
    }

    def __init__(self, proto_file: str, include_paths: Optional[List[str]] = None):
        self.proto_file = proto_file
        self.include_paths = include_paths or []
        self.file_descriptor: Optional[descriptor_pb2.FileDescriptorProto] = None
        self.registry: Optional[DescriptorRegistry] = None
        self._random = random.Random()
        self._load_proto()

    def _load_proto(self) -> None:
        """Load and compile the proto file."""

        self._ensure_validate_pb2()
        _load_validate_pb2()

        proto_abs_path = os.path.abspath(self.proto_file)
        proto_dir = os.path.dirname(proto_abs_path)
        proto_basename = os.path.basename(proto_abs_path)

        search_paths = [
            proto_dir,
            os.path.join(os.path.dirname(__file__), 'proto'),
        ] + [os.path.abspath(path) for path in self.include_paths]

        with TemporaryDirectory() as tmpdir:
            desc_file = os.path.join(tmpdir, 'descriptor.pb')
            protoc_args = [
                'protoc',
                '--descriptor_set_out=' + desc_file,
                '--include_imports',
            ]

            for path in search_paths:
                if os.path.exists(path):
                    protoc_args.append('-I' + path)

            protoc_args.append(proto_basename)

            old_cwd = os.getcwd()
            try:
                os.chdir(proto_dir)
                status = invoke_protoc(protoc_args)
            finally:
                os.chdir(old_cwd)

            if status != 0:
                raise RuntimeError(f"protoc failed with status {status}")

            with open(desc_file, 'rb') as handle:
                descriptor_data = handle.read()

        file_set = descriptor_pb2.FileDescriptorSet()
        file_set.ParseFromString(descriptor_data)

        for file_desc in file_set.file:
            if file_desc.name == proto_basename or file_desc.name.endswith('/' + proto_basename):
                self.file_descriptor = file_desc
                break

        if not self.file_descriptor:
            raise ValueError(f"Could not find descriptor for {proto_basename}")

        self.registry = DescriptorRegistry(file_set, self.file_descriptor)

    def _ensure_validate_pb2(self) -> None:
        """Ensure generator/proto/validate_pb2.py exists by building it if missing."""

        try:
            gen_dir = os.path.dirname(os.path.abspath(__file__))
            proto_dir = os.path.join(gen_dir, 'proto')
            target_py = os.path.join(proto_dir, 'validate_pb2.py')
            if os.path.isfile(target_py):
                return

            validate_proto = os.path.join(proto_dir, 'validate.proto')
            if not os.path.isfile(validate_proto):
                return

            protoc_args = [
                'protoc',
                f'--python_out={proto_dir}',
                f'-I{proto_dir}',
                os.path.basename(validate_proto),
            ]

            old_cwd = os.getcwd()
            try:
                os.chdir(proto_dir)
                invoke_protoc(protoc_args)
            finally:
                os.chdir(old_cwd)
        except Exception:
            pass

    def get_messages(self) -> List[str]:
        """Get list of top-level messages in the target proto file."""

        return list(self.registry.target_top_level_names if self.registry else [])

    def get_field_info(self, message_name: str, field_name: str) -> Optional[ProtoFieldInfo]:
        """Get field information for a specific message field."""

        try:
            message_info = self.registry.resolve_message(message_name)
        except ValueError:
            return None
        return message_info.fields.get(field_name)

    def get_all_fields(self, message_name: str) -> Dict[str, ProtoFieldInfo]:
        """Get all fields for a message."""

        try:
            return dict(self.registry.resolve_message(message_name).fields)
        except ValueError:
            return {}

    def generate_valid(self, message_name: str, seed: Optional[int] = None) -> Dict[str, Any]:
        """Generate valid test data for a message."""

        self._random = random.Random(seed)
        message_info = self.registry.resolve_message(message_name)
        return self._generate_message(message_info.full_name, (message_info.full_name,))

    def generate_invalid(
        self,
        message_name: str,
        violate_field: Optional[Union[str, List[str]]] = None,
        violate_rule: Optional[Union[str, List[str]]] = None,
        seed: Optional[int] = None
    ) -> Dict[str, Any]:
        """Generate invalid test data for a message."""

        self._random = random.Random(seed)
        message_info = self.registry.resolve_message(message_name)
        data = self._generate_message(message_info.full_name, (message_info.full_name,))
        fields = message_info.fields

        constrained_fields = {
            name: info for name, info in fields.items() if info.constraints
        }
        if not constrained_fields:
            raise ValueError(f"No validation constraints found for {message_name}")

        if violate_field is None:
            selected_fields = [self._random.choice(list(constrained_fields.keys()))]
        elif isinstance(violate_field, str):
            selected_fields = [field.strip() for field in violate_field.split(',') if field.strip()]
        else:
            selected_fields = []
            for field_name in violate_field:
                if isinstance(field_name, str):
                    selected_fields.extend(
                        [part.strip() for part in field_name.split(',') if part.strip()]
                    )

        for field_name in selected_fields:
            if field_name not in constrained_fields:
                if field_name not in fields:
                    raise ValueError(f"Field {field_name} does not exist in message {message_name}")
                raise ValueError(f"Field {field_name} has no constraints to violate")

        if violate_rule:
            if isinstance(violate_rule, str):
                candidate_rules = [rule.strip() for rule in violate_rule.split(',') if rule.strip()]
            else:
                candidate_rules = []
                for rule_name in violate_rule:
                    if isinstance(rule_name, str):
                        candidate_rules.extend(
                            [part.strip() for part in rule_name.split(',') if part.strip()]
                        )
        else:
            candidate_rules = []

        for field_name in selected_fields:
            field_info = constrained_fields[field_name]
            if candidate_rules:
                matching_constraints = [
                    constraint
                    for constraint in field_info.constraints
                    if constraint.rule_type in set(candidate_rules)
                ]
                chosen = self._random.choice(matching_constraints or field_info.constraints)
            else:
                chosen = self._random.choice(field_info.constraints)

            data[field_name] = self._generate_invalid_value(
                field_info,
                chosen,
                (message_info.full_name,),
            )

        return data

    def _generate_message(
        self,
        message_full_name: str,
        active_stack: Tuple[str, ...],
    ) -> Dict[str, Any]:
        """Generate a valid data dictionary for a message descriptor."""

        message_info = self.registry.resolve_message(message_full_name)
        data: Dict[str, Any] = {}
        for field_info in message_info.fields.values():
            value = self._generate_valid_value(field_info, active_stack)
            if value is not None:
                data[field_info.name] = value
        return data

    def _generate_valid_value(
        self,
        field_info: ProtoFieldInfo,
        active_stack: Tuple[str, ...],
    ) -> Any:
        """Generate a valid value for a field."""

        if field_info.is_repeated():
            return self._generate_valid_repeated(field_info, active_stack)

        constraints = field_info.constraint_map()
        type_name = field_info.get_type_name()

        scalar_generators = {
            'int32': self._generate_valid_int32,
            'int64': self._generate_valid_int64,
            'uint32': self._generate_valid_uint32,
            'uint64': self._generate_valid_uint64,
            'sint32': self._generate_valid_int32,
            'sint64': self._generate_valid_int64,
            'fixed32': self._generate_valid_uint32,
            'fixed64': self._generate_valid_uint64,
            'sfixed32': self._generate_valid_int32,
            'sfixed64': self._generate_valid_int64,
            'float': self._generate_valid_float,
            'double': self._generate_valid_double,
            'bool': self._generate_valid_bool,
            'string': self._generate_valid_string,
            'bytes': self._generate_valid_bytes,
        }

        if type_name in scalar_generators:
            return scalar_generators[type_name](constraints)
        if type_name == 'enum':
            return self._generate_valid_enum(field_info, constraints)
        if type_name == 'message':
            return self._generate_valid_message_field(field_info, active_stack)
        return None

    def _generate_valid_message_field(
        self,
        field_info: ProtoFieldInfo,
        active_stack: Tuple[str, ...],
    ) -> Any:
        """Generate a valid nested message or Any field."""

        if field_info.is_any():
            return self._generate_valid_any(field_info, active_stack)

        full_name = field_info.full_type_name()
        if not full_name:
            return None
        if full_name in active_stack or len(active_stack) >= self.MAX_MESSAGE_DEPTH:
            return None
        return self._generate_message(full_name, active_stack + (full_name,))

    def _generate_valid_any(
        self,
        field_info: ProtoFieldInfo,
        active_stack: Tuple[str, ...],
    ) -> Dict[str, Any]:
        """Generate a valid google.protobuf.Any payload."""

        candidates = self._get_any_candidates(field_info, active_stack)
        if not candidates:
            raise ValueError(f"No suitable concrete message found for Any field {field_info.name}")

        remaining = list(candidates)
        while remaining:
            candidate = remaining.pop(self._random.randrange(len(remaining)))
            try:
                payload_data = self._generate_message(
                    candidate.message_info.full_name,
                    active_stack + (candidate.message_info.full_name,),
                )
                payload_message = self._build_message_from_data(
                    candidate.message_info.full_name,
                    payload_data,
                )
            except ValueError:
                continue

            return {
                'type_url': candidate.type_url,
                'value': payload_message.SerializeToString(),
            }

        raise ValueError(f"No suitable concrete message found for Any field {field_info.name}")

    def _get_any_candidates(
        self,
        field_info: ProtoFieldInfo,
        active_stack: Tuple[str, ...],
    ) -> List[AnyCandidate]:
        """Resolve valid Any payload candidates."""

        constraints = field_info.constraint_map()
        allowed_urls = list(constraints.get('in') or [])
        disallowed_urls = list(constraints.get('not_in') or [])
        disallowed_type_names = {self._extract_type_name(url) for url in disallowed_urls}

        candidates: List[AnyCandidate] = []
        if allowed_urls:
            for type_url in allowed_urls:
                message_info = self._resolve_any_type_url(type_url)
                if not message_info:
                    continue
                if message_info.full_name in active_stack:
                    continue
                if not self._is_any_candidate_message(message_info):
                    continue
                candidates.append(AnyCandidate(message_info=message_info, type_url=type_url))
            return candidates

        for message_info in self.registry.iter_messages():
            if not self._is_any_candidate_message(message_info):
                continue
            if message_info.full_name in active_stack:
                continue
            if message_info.full_name in disallowed_type_names:
                continue
            type_url = self._canonical_type_url(message_info.full_name)
            if type_url in disallowed_urls:
                continue
            candidates.append(AnyCandidate(message_info=message_info, type_url=type_url))

        return candidates

    def _is_any_candidate_message(self, message_info: MessageInfo) -> bool:
        """Check whether a message is a suitable Any payload candidate."""

        if message_info.descriptor_proto.options.map_entry:
            return False
        if message_info.full_name == 'google.protobuf.Any':
            return False
        if message_info.file_descriptor.name in self._SYSTEM_ANY_EXCLUDED_FILES:
            return False
        return True

    def _resolve_any_type_url(self, type_url: str) -> Optional[MessageInfo]:
        """Resolve a type_url to a concrete message."""

        type_name = self._extract_type_name(type_url)
        try:
            return self.registry.resolve_message(type_name)
        except ValueError:
            return None

    @staticmethod
    def _extract_type_name(type_url: str) -> str:
        """Extract the fully qualified protobuf type name from a type URL."""

        return str(type_url).rsplit('/', 1)[-1].lstrip('.')

    @staticmethod
    def _canonical_type_url(full_name: str) -> str:
        """Build the canonical type_url for a message."""

        return f"type.googleapis.com/{full_name.lstrip('.')}"

    def _generate_valid_int32(self, constraints: Dict[str, Any]) -> int:
        min_val = -(2**31)
        max_val = 2**31 - 1

        if 'gte' in constraints:
            min_val = max(min_val, int(constraints['gte']))
        elif 'gt' in constraints:
            min_val = max(min_val, int(constraints['gt']) + 1)

        if 'lte' in constraints:
            max_val = min(max_val, int(constraints['lte']))
        elif 'lt' in constraints:
            max_val = min(max_val, int(constraints['lt']) - 1)

        if 'const' in constraints:
            return int(constraints['const'])
        if 'in' in constraints:
            return self._random.choice(list(constraints['in']))

        return self._random.randint(min_val, max_val)

    def _generate_valid_int64(self, constraints: Dict[str, Any]) -> int:
        min_val = -(2**63)
        max_val = 2**63 - 1

        if 'gte' in constraints:
            min_val = max(min_val, int(constraints['gte']))
        elif 'gt' in constraints:
            min_val = max(min_val, int(constraints['gt']) + 1)

        if 'lte' in constraints:
            max_val = min(max_val, int(constraints['lte']))
        elif 'lt' in constraints:
            max_val = min(max_val, int(constraints['lt']) - 1)

        if 'const' in constraints:
            return int(constraints['const'])
        if 'in' in constraints:
            return self._random.choice(list(constraints['in']))

        if max_val - min_val > 10**9:
            return self._random.randint(min_val, min(min_val + 10**9, max_val))
        return self._random.randint(min_val, max_val)

    def _generate_valid_uint32(self, constraints: Dict[str, Any]) -> int:
        min_val = 0
        max_val = 2**32 - 1

        if 'gte' in constraints:
            min_val = max(min_val, int(constraints['gte']))
        elif 'gt' in constraints:
            min_val = max(min_val, int(constraints['gt']) + 1)

        if 'lte' in constraints:
            max_val = min(max_val, int(constraints['lte']))
        elif 'lt' in constraints:
            max_val = min(max_val, int(constraints['lt']) - 1)

        if 'const' in constraints:
            return int(constraints['const'])
        if 'in' in constraints:
            return self._random.choice(list(constraints['in']))

        return self._random.randint(min_val, max_val)

    def _generate_valid_uint64(self, constraints: Dict[str, Any]) -> int:
        min_val = 0
        max_val = 2**64 - 1

        if 'gte' in constraints:
            min_val = max(min_val, int(constraints['gte']))
        elif 'gt' in constraints:
            min_val = max(min_val, int(constraints['gt']) + 1)

        if 'lte' in constraints:
            max_val = min(max_val, int(constraints['lte']))
        elif 'lt' in constraints:
            max_val = min(max_val, int(constraints['lt']) - 1)

        if 'const' in constraints:
            return int(constraints['const'])
        if 'in' in constraints:
            return self._random.choice(list(constraints['in']))

        if max_val - min_val > 10**9:
            return self._random.randint(min_val, min(min_val + 10**9, max_val))
        return self._random.randint(min_val, max_val)

    def _generate_valid_float(self, constraints: Dict[str, Any]) -> float:
        min_val = -3.4e38
        max_val = 3.4e38

        if 'gte' in constraints:
            min_val = max(min_val, float(constraints['gte']))
        elif 'gt' in constraints:
            min_val = max(min_val, float(constraints['gt']) + 0.01)

        if 'lte' in constraints:
            max_val = min(max_val, float(constraints['lte']))
        elif 'lt' in constraints:
            max_val = min(max_val, float(constraints['lt']) - 0.01)

        if 'const' in constraints:
            return float(constraints['const'])
        return self._random.uniform(min_val, max_val)

    def _generate_valid_double(self, constraints: Dict[str, Any]) -> float:
        min_val = -1.7e308
        max_val = 1.7e308

        if 'gte' in constraints:
            min_val = max(min_val, float(constraints['gte']))
        elif 'gt' in constraints:
            min_val = max(min_val, float(constraints['gt']) + 0.01)

        if 'lte' in constraints:
            max_val = min(max_val, float(constraints['lte']))
        elif 'lt' in constraints:
            max_val = min(max_val, float(constraints['lt']) - 0.01)

        if 'const' in constraints:
            return float(constraints['const'])
        return self._random.uniform(min_val, max_val)

    def _generate_valid_bool(self, constraints: Dict[str, Any]) -> bool:
        if 'const' in constraints:
            return bool(constraints['const'])
        return self._random.choice([True, False])

    def _generate_valid_enum(self, field_info: ProtoFieldInfo, constraints: Dict[str, Any]) -> int:
        """Generate a valid enum value."""

        if 'const' in constraints:
            return int(constraints['const'])
        if 'in' in constraints:
            return int(self._random.choice(list(constraints['in'])))

        runtime_descriptor = self.registry.pool.FindEnumTypeByName(field_info.full_type_name())
        values = [value.number for value in runtime_descriptor.values]
        if 'not_in' in constraints:
            forbidden = {int(value) for value in constraints['not_in']}
            values = [value for value in values if value not in forbidden]
        if not values:
            raise ValueError(f"No usable enum values for field {field_info.name}")
        return self._random.choice(values)

    def _generate_valid_string(self, constraints: Dict[str, Any]) -> str:
        min_len = constraints.get('min_len', 1)
        max_len = constraints.get('max_len', 20)

        if 'const' in constraints:
            return constraints['const']
        if 'in' in constraints:
            return self._random.choice(list(constraints['in']))

        try:
            if constraints.get('email'):
                return self._generate_valid_email()
            if constraints.get('hostname'):
                return self._generate_valid_hostname()
            if constraints.get('ipv4'):
                return self._generate_valid_ipv4()
            if constraints.get('ipv6'):
                return self._generate_valid_ipv6()
            if constraints.get('ip'):
                return (
                    self._generate_valid_ipv4()
                    if self._random.choice([True, False])
                    else self._generate_valid_ipv6()
                )
        except Exception:
            pass

        length = max(min_len, min(max_len, self._random.randint(min_len, max_len)))

        if constraints.get('ascii', False):
            chars = string.ascii_letters + string.digits
        else:
            chars = string.ascii_letters + string.digits + string.punctuation

        base_str = ''.join(self._random.choice(chars) for _ in range(max(1, length)))

        if 'prefix' in constraints:
            prefix = constraints['prefix']
            remaining = max(0, max_len - len(prefix))
            base_str = (prefix + base_str[:remaining]) if remaining > 0 else prefix

        if 'suffix' in constraints:
            suffix = constraints['suffix']
            available = max(0, max_len - len(suffix))
            base_str = (base_str[:available] + suffix) if available > 0 else suffix

        if 'contains' in constraints:
            contains = constraints['contains']
            if contains not in base_str:
                if len(base_str) + len(contains) <= max_len:
                    pos = self._random.randint(0, len(base_str))
                    base_str = base_str[:pos] + contains + base_str[pos:]
                else:
                    pos = self._random.randint(0, max(0, len(base_str) - len(contains)))
                    base_str = base_str[:pos] + contains + base_str[pos + len(contains):]

        if 'not_in' in constraints:
            forbidden = set(constraints['not_in']) if isinstance(constraints['not_in'], list) else {constraints['not_in']}
            attempts = 0
            while base_str in forbidden and attempts < 10:
                base_str = ''.join(self._random.choice(chars) for _ in range(max(1, length)))
                attempts += 1

        if len(base_str) < min_len:
            base_str += ''.join(self._random.choice(chars) for _ in range(min_len - len(base_str)))
        if len(base_str) > max_len:
            base_str = base_str[:max_len]

        return base_str

    def _generate_valid_email(self) -> str:
        lp_len = self._random.randint(1, 16)
        lp_chars = string.ascii_letters + string.digits + '._-'
        local = ''.join(self._random.choice(lp_chars) for _ in range(lp_len))
        local = local.strip('.')
        if not local:
            local = 'u'
        return f"{local}@{self._generate_valid_hostname()}"

    def _generate_valid_hostname(self) -> str:
        labels = []
        num_labels = self._random.randint(2, 4)
        for _ in range(num_labels):
            length = self._random.randint(1, 12)
            chars = string.ascii_lowercase + string.digits + '-'
            label = ''.join(self._random.choice(chars) for _ in range(length))
            if label[0] == '-':
                label = 'a' + label[1:]
            if label[-1] == '-':
                label = label[:-1] + 'z'
            labels.append(label)
        return '.'.join(labels)[:253]

    def _generate_valid_ipv4(self) -> str:
        return '.'.join(str(self._random.randint(0, 255)) for _ in range(4))

    def _generate_valid_ipv6(self) -> str:
        def hextet() -> str:
            return ''.join(
                self._random.choice('0123456789abcdef')
                for _ in range(self._random.randint(1, 4))
            )
        return ':'.join(hextet() for _ in range(8))

    def _generate_valid_bytes(self, constraints: Dict[str, Any]) -> bytes:
        min_len = constraints.get('min_len', 1)
        max_len = constraints.get('max_len', 20)

        if 'const' in constraints:
            return constraints['const']

        length = self._random.randint(min_len, max_len)
        return bytes(self._random.randint(0, 255) for _ in range(length))

    def _generate_valid_repeated(
        self,
        field_info: ProtoFieldInfo,
        active_stack: Tuple[str, ...],
    ) -> List[Any]:
        """Generate a valid repeated field value."""

        constraints = field_info.constraint_map()
        min_items = constraints.get('min_items', 1)
        max_items = constraints.get('max_items', 5)
        count = self._random.randint(min_items, max_items)

        items: List[Any] = []
        attempts = 0
        max_attempts = max(count * 3, 1)
        while len(items) < count and attempts < max_attempts:
            item = self._generate_valid_single_item(field_info, active_stack)
            attempts += 1
            if item is None:
                continue
            if constraints.get('unique', False):
                if self._contains_equivalent(items, item):
                    continue
            items.append(item)

        return items

    def _generate_valid_single_item(
        self,
        field_info: ProtoFieldInfo,
        active_stack: Tuple[str, ...],
    ) -> Any:
        """Generate a single repeated-field item."""

        pseudo_field = ProtoFieldInfo(field_info.descriptor)
        pseudo_field.label = descriptor_pb2.FieldDescriptorProto.LABEL_OPTIONAL
        pseudo_field.constraints = [
            constraint
            for constraint in field_info.constraints
            if constraint.rule_type not in ('min_items', 'max_items', 'unique')
        ]
        return self._generate_valid_value(pseudo_field, active_stack)

    @staticmethod
    def _contains_equivalent(items: Sequence[Any], candidate: Any) -> bool:
        """Check whether a candidate already exists in a list."""

        for item in items:
            if item == candidate:
                return True
        return False

    def _generate_invalid_value(
        self,
        field_info: ProtoFieldInfo,
        constraint: ValidationConstraint,
        active_stack: Tuple[str, ...],
    ) -> Any:
        """Generate an invalid value that violates the given constraint."""

        rule_type = constraint.rule_type
        rule_value = constraint.value
        type_name = field_info.get_type_name()

        if field_info.is_any():
            return self._generate_invalid_any_value(field_info, rule_type, rule_value, active_stack)

        if rule_type in ('gt', 'gte'):
            if type_name.startswith('int') or type_name.startswith('uint') or type_name.startswith('sint') or type_name.startswith('fixed') or type_name == 'enum':
                return int(rule_value) - self._random.randint(1, 100)
            return float(rule_value) - self._random.uniform(0.1, 10)

        if rule_type in ('lt', 'lte'):
            if type_name.startswith('int') or type_name.startswith('uint') or type_name.startswith('sint') or type_name.startswith('fixed') or type_name == 'enum':
                return int(rule_value) + self._random.randint(1, 100)
            return float(rule_value) + self._random.uniform(0.1, 10)

        if rule_type == 'const':
            if type_name in ('int32', 'int64', 'uint32', 'uint64', 'sint32', 'sint64', 'fixed32', 'fixed64', 'sfixed32', 'sfixed64', 'enum'):
                return int(rule_value) + self._random.randint(1, 100)
            if type_name in ('float', 'double'):
                return float(rule_value) + self._random.uniform(1.0, 10.0)
            if type_name == 'bool':
                return not rule_value
            if type_name == 'string':
                return str(rule_value) + "_invalid"
            if type_name == 'bytes':
                return bytes(rule_value) + b'_invalid'
            return None

        if rule_type == 'min_len':
            if type_name == 'bytes':
                return b'x' * max(int(rule_value) - 1, 0)
            return 'x' * max(int(rule_value) - 1, 0)

        if rule_type == 'max_len':
            if type_name == 'bytes':
                return b'x' * (int(rule_value) + 10)
            return 'x' * (int(rule_value) + 10)

        if rule_type == 'prefix':
            return 'WRONG_' + str(rule_value)

        if rule_type == 'suffix':
            return str(rule_value) + '_WRONG'

        if rule_type == 'contains':
            return 'DOES_NOT_CONTAIN_REQUIRED'

        if rule_type == 'ascii':
            return 'test_中文_invalid'

        if rule_type == 'email':
            return 'invalid-email-without-at'

        if rule_type == 'hostname':
            return '-bad_host_name-'

        if rule_type == 'ip':
            return '999.999.999.999'

        if rule_type == 'ipv4':
            return '256.300.1.2'

        if rule_type == 'ipv6':
            return 'gggg:gggg:gggg:gggg:gggg:gggg:gggg:gggg'

        if rule_type == 'in':
            if type_name == 'string':
                return 'not_in_list_' + str(self._random.randint(1, 1000))
            if type_name == 'bytes':
                return b'not_in_list'
            return 999999

        if rule_type == 'not_in':
            if isinstance(rule_value, list) and rule_value:
                return rule_value[0]
            return rule_value

        if rule_type == 'min_items':
            return []

        if rule_type == 'max_items':
            return [
                self._generate_valid_single_item(field_info, active_stack)
                for _ in range(int(rule_value) + 5)
            ]

        if rule_type == 'unique':
            item = self._generate_valid_single_item(field_info, active_stack)
            return [item, item, item]

        return None

    def _generate_invalid_any_value(
        self,
        field_info: ProtoFieldInfo,
        rule_type: str,
        rule_value: Any,
        active_stack: Tuple[str, ...],
    ) -> Dict[str, Any]:
        """Generate an invalid Any value for type allow/deny-list rules."""

        if rule_type == 'in':
            allowed_type_names = {self._extract_type_name(url) for url in rule_value}
            for candidate in self._get_any_candidates(field_info, active_stack):
                if candidate.message_info.full_name not in allowed_type_names:
                    payload = self._generate_message(
                        candidate.message_info.full_name,
                        active_stack + (candidate.message_info.full_name,),
                    )
                    payload_message = self._build_message_from_data(candidate.message_info.full_name, payload)
                    return {
                        'type_url': candidate.type_url,
                        'value': payload_message.SerializeToString(),
                    }
            return {
                'type_url': 'type.googleapis.com/invalid.Payload',
                'value': b'',
            }

        if rule_type == 'not_in':
            if not rule_value:
                return {'type_url': 'type.googleapis.com/invalid.Payload', 'value': b''}
            disallowed_url = rule_value[0]
            message_info = self._resolve_any_type_url(disallowed_url)
            if message_info and message_info.full_name not in active_stack:
                payload = self._generate_message(
                    message_info.full_name,
                    active_stack + (message_info.full_name,),
                )
                payload_message = self._build_message_from_data(message_info.full_name, payload)
                return {
                    'type_url': disallowed_url,
                    'value': payload_message.SerializeToString(),
                }
            return {'type_url': disallowed_url, 'value': b''}

        return {'type_url': 'type.googleapis.com/invalid.Payload', 'value': b''}

    def encode_to_binary(self, message_name: str, data: Dict[str, Any]) -> bytes:
        """Encode a data dictionary to protobuf binary format."""

        message = self._build_message_from_data(message_name, data)
        return message.SerializeToString()

    def _build_message_from_data(self, message_name: str, data: Dict[str, Any]) -> Message:
        """Build a dynamic protobuf message from generated data."""

        message_class = self.registry.get_message_class(message_name)
        message = message_class()
        self._populate_message(message, data)
        return message

    def _populate_message(self, message: Message, data: Dict[str, Any]) -> None:
        """Populate a protobuf message instance from a dictionary."""

        for field_name, value in data.items():
            runtime_field = message.DESCRIPTOR.fields_by_name.get(field_name)
            if runtime_field is None or value is None:
                continue

            if runtime_field.is_repeated:
                if runtime_field.cpp_type == RuntimeFieldDescriptor.CPPTYPE_MESSAGE:
                    container = getattr(message, field_name)
                    for item in value:
                        if item is None:
                            continue
                        if runtime_field.message_type.full_name == 'google.protobuf.Any':
                            container.add().CopyFrom(
                                self._build_runtime_any_message(runtime_field.message_type, item)
                            )
                        else:
                            container.add().CopyFrom(
                                self._build_runtime_message(runtime_field.message_type, item)
                            )
                else:
                    getattr(message, field_name).extend(value)
                continue

            if runtime_field.cpp_type == RuntimeFieldDescriptor.CPPTYPE_MESSAGE:
                if runtime_field.message_type.full_name == 'google.protobuf.Any':
                    getattr(message, field_name).CopyFrom(
                        self._build_runtime_any_message(runtime_field.message_type, value)
                    )
                else:
                    getattr(message, field_name).CopyFrom(
                        self._build_runtime_message(runtime_field.message_type, value)
                    )
            else:
                setattr(message, field_name, value)

    def _build_runtime_message(
        self,
        descriptor: RuntimeDescriptor,
        value: Union[Dict[str, Any], Message],
    ) -> Message:
        """Build a runtime submessage from a dict or pass through a message."""

        if isinstance(value, Message):
            return value

        message_class = self.registry.get_message_class_for_descriptor(descriptor)
        message = message_class()
        self._populate_message(message, value)
        return message

    def _normalize_any_value(self, value: Union[Dict[str, Any], any_pb2.Any]) -> Dict[str, Any]:
        """Normalize Any input into a type_url/value dictionary."""

        if isinstance(value, any_pb2.Any):
            return {'type_url': value.type_url, 'value': value.value}

        if not isinstance(value, dict):
            raise TypeError("Any field value must be a dict or Any instance")

        if '@type' in value:
            type_url = str(value['@type'])
            payload = value.get('value', {})
            if isinstance(payload, dict):
                message_info = self._resolve_any_type_url(type_url)
                if not message_info:
                    raise ValueError(f"Unknown Any type_url: {type_url}")
                payload_message = self._build_message_from_data(message_info.full_name, payload)
                payload_bytes = payload_message.SerializeToString()
            else:
                payload_bytes = payload
            return {'type_url': type_url, 'value': payload_bytes}

        type_url = value.get('type_url')
        payload_value = value.get('value', b'')
        if type_url is None:
            raise ValueError("Any field value must contain type_url")

        if isinstance(payload_value, dict):
            message_info = self._resolve_any_type_url(str(type_url))
            if not message_info:
                raise ValueError(f"Unknown Any type_url: {type_url}")
            payload_message = self._build_message_from_data(message_info.full_name, payload_value)
            payload_bytes = payload_message.SerializeToString()
        else:
            payload_bytes = payload_value

        return {'type_url': str(type_url), 'value': payload_bytes}

    def _build_runtime_any_message(
        self,
        descriptor: RuntimeDescriptor,
        value: Union[Dict[str, Any], any_pb2.Any],
    ) -> Message:
        """Build an Any message instance tied to a descriptor pool."""

        normalized = self._normalize_any_value(value)
        return self._build_runtime_message(descriptor, normalized)

    def format_output(
        self,
        data: Any,
        format_type: OutputFormat = OutputFormat.C_ARRAY,
        name: str = "test_data"
    ) -> Union[str, bytes]:
        """Format generated output."""

        if format_type == OutputFormat.BINARY:
            return data

        if format_type == OutputFormat.HEX_STRING:
            return data.hex()

        if format_type == OutputFormat.C_ARRAY:
            hex_values = ', '.join(f'0x{byte:02x}' for byte in data)
            return (
                f'const uint8_t {name}[] = {{{hex_values}}};\n'
                f'const size_t {name}_size = {len(data)};'
            )

        if format_type == OutputFormat.PYTHON_DICT:
            return str(data)

        return str(data)


def main() -> int:
    """Command line entry point."""

    import argparse

    parser = argparse.ArgumentParser(
        description='Generate test data for protobuf messages with validation'
    )
    parser.add_argument('proto_file', help='Path to .proto file')
    parser.add_argument('message', help='Message name to generate data for')
    parser.add_argument('--invalid', action='store_true',
                        help='Generate invalid data instead of valid')
    parser.add_argument(
        '--field', dest='fields', action='append', default=None,
        help='Field(s) to violate (can be given multiple times or comma-separated)'
    )
    parser.add_argument(
        '--rule', dest='rules', action='append', default=None,
        help='Rule(s) to violate (can be given multiple times or comma-separated)'
    )
    parser.add_argument('--format', choices=['binary', 'c_array', 'hex', 'dict'],
                        default='c_array', help='Output format')
    parser.add_argument('--output', '-o', help='Output file (default: stdout)')
    parser.add_argument('--seed', type=int, help='Random seed for reproducibility')
    parser.add_argument('-I', '--include', action='append', default=[],
                        help='Include path for proto files')

    args = parser.parse_args()

    try:
        generator = DataGenerator(args.proto_file, args.include)
    except Exception as exc:
        print(f"Error loading proto file: {exc}", file=sys.stderr)
        return 1

    try:
        if args.invalid:
            data_dict = generator.generate_invalid(
                args.message,
                violate_field=args.fields,
                violate_rule=args.rules,
                seed=args.seed
            )
        else:
            data_dict = generator.generate_valid(args.message, seed=args.seed)

        format_map = {
            'binary': OutputFormat.BINARY,
            'c_array': OutputFormat.C_ARRAY,
            'hex': OutputFormat.HEX_STRING,
            'dict': OutputFormat.PYTHON_DICT,
        }

        if args.format == 'dict':
            output = generator.format_output(
                data_dict,
                format_map[args.format],
                f"{args.message.lower()}_data"
            )
        else:
            binary_data = generator.encode_to_binary(args.message, data_dict)
            output = generator.format_output(
                binary_data,
                format_map[args.format],
                f"{args.message.lower()}_data"
            )

        if args.output:
            if args.format == 'binary':
                with open(args.output, 'wb') as handle:
                    handle.write(output)
            else:
                with open(args.output, 'w') as handle:
                    handle.write(output)
                    handle.write('\n')
        else:
            if args.format == 'binary':
                sys.stdout.buffer.write(output)
            else:
                print(output)

        print(f"\n// Generated data: {data_dict}", file=sys.stderr)
    except Exception as exc:
        print(f"Error generating data: {exc}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
