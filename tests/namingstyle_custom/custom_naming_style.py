import re


class CustomNamingStyle:
    """A custom C naming style which fewer delimiters."""

    def struct_name(self, name):
        return "_" + self._pascal_case(name)

    def define_name(self, name):
        return self._snake_case(name).upper()

    def union_name(self, name):
        return self.struct_name(name)

    def enum_name(self, name):
        return self.struct_name(name)

    def enum_entry(self, name):
        return self.define_name(name)

    def var_name(self, name):
        return self._camel_case(name)

    def func_name(self, name):
        return self.type_name(name)

    def type_name(self, name):
        return self._pascal_case(name)

    def bytes_type(self, struct_name, name):
        return self._pascal_case("%s_%s" % (struct_name, name)) + "_t"

    def _pascal_case(self, name):
        return self._snake_case(name).title().replace('_', '')

    def _camel_case(self, name):
        name = self._pascal_case(name)
        return name[:1].lower() + name[1:]

    def _snake_case(self, name):
        name = str(name)

        # Insert a boundary before the last uppercase letter in a series of uppercase letters if lowercase letters follow
        name = re.sub(r"([A-Z]+)([A-Z][a-z])", r'\1_\2', name)

        # Insert a boundary between lowercase letters followed by uppercase letters
        name = re.sub(r"([a-z\d])([A-Z])", r'\1_\2', name)

        # Replace all delimiters with underscores
        name = re.sub(r"[_ -]+", "_", name)

        return name.lower()
