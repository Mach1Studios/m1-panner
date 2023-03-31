#ifndef M1_NOTEPAD_ENVIRONMENTVARIABLE_H
#define M1_NOTEPAD_ENVIRONMENTVARIABLE_H

#include <string>
#include <iostream>
#include <array>
#include <vector>

namespace GA {
	class IEnvironmentHandler {
	public:
		virtual ~IEnvironmentHandler() = default;
		/**
		 * Sets an environment variable
		 * @param variable_name Variable name
		 * @param variable_value Variable value
		 */
		virtual void SetEnvVariable(std::string variable_name, std::string variable_value) = 0;

		/**
		 * Remove an environment variable
		 */
		virtual int RemoveEnvVariable(std::string variable_name) = 0;

		/**
		 * Retrieve environment variable
		 * @param variable_name Requested variable
		 * @return String containing the variable value
		 */
		virtual std::string GetEnvVariable(std::string variable_name) = 0;
	};

#ifdef _WIN32

	class EnvironmentHandler : public IEnvironmentHandler {
		void CheckError() const
		{
			if (errno) {
				std::array<char, 1024> buffer{};
				std::cout << strerror_s(buffer.data(), buffer.size(), errno);
			}
		}

	public:
		void SetEnvVariable(std::string variable_name, std::string variable_value) override {
			_putenv_s(variable_name.c_str(), variable_value.c_str());
			CheckError();
		}

		int RemoveEnvVariable(std::string variable_name) override {
			const auto result = _putenv_s(variable_name.c_str(), "");
			CheckError();
			return result;
		}

		std::string GetEnvVariable(std::string variable_name) override {
			std::vector<char> to_return{};
			// Used to query the
			size_t required_size;

			// TODO: Expand class to used proper *_s functions on Windows.

			getenv_s(&required_size,
				nullptr,
				0 /* Buffer count*/,
				variable_name.c_str());

			if (required_size == 0) {
				// Envvar was not found
				return {};
			}

			to_return.resize(required_size);

			getenv_s(&required_size,
				to_return.data(),
				to_return.size(),
				variable_name.c_str());

			return { to_return.begin(), to_return.end() };
		}
	};

#elif defined(__APPLE__)

#endif
}

#endif //M1_NOTEPAD_ENVIRONMENTVARIABLE_H
