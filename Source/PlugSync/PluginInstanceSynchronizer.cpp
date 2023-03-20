#include "PluginInstanceSynchronizer.h"

#include <array>

#ifdef _WIN32

#include <Windows.h>

#else

#endif


template<typename Type>
void ReadStoredEnvironmentVariable(const std::string &env_var_buffer, void *env_ptr, Type *&local_member_to_store_in) {
#ifdef _WIN32
    sscanf_s(env_var_buffer.c_str(), "%p", &env_ptr);
#elif defined (__APPLE__)
    sscanf(env_var_buffer.c_str(), "%p", &env_ptr);
#endif
    local_member_to_store_in = static_cast<Type *>(env_ptr);
}

PluginInstanceSynchronizer::PluginInstanceSynchronizer() {
    const auto counter_env_var_res = m_env_handler.GetEnvVariable(EnvironmentVariableCounter);
    const auto counter_mtx_env_var_res = m_env_handler.GetEnvVariable(EnvironmentVariableMutex);
    const auto session_id_env_var_res = m_env_handler.GetEnvVariable(EnvironmentVariableSessionIdentifier);
    const auto session_flag_env_var_res = m_env_handler.GetEnvVariable(EnvironmentVariableSessionFlag);

    std::array<char, 128> env_ptr_buf{};
    void *env_ptr = nullptr;

    if (counter_env_var_res.empty() || counter_mtx_env_var_res.empty()) {
        // Initialize the instance counter
        this->m_instance_counter = new size_t(1);
        this->m_counter_mtx = new std::mutex();

        // Now set the addresses in the environment vars
        snprintf(env_ptr_buf.data(), env_ptr_buf.size(), "%p", m_instance_counter);
        m_env_handler.SetEnvVariable(EnvironmentVariableCounter, env_ptr_buf.data());

        snprintf(env_ptr_buf.data(), env_ptr_buf.size(), "%p", m_counter_mtx);
        m_env_handler.SetEnvVariable(EnvironmentVariableMutex, env_ptr_buf.data());

        // We purposefully don't initialize the m_plugin_instance_id with specific data,
        // because this is called in the AudioProcessor's ctor
        // The state restoration has not happened yet & we can't verify
        this->m_plugin_instance_id = new std::string();

        // We mark this later on when we receive the id
        this->m_identificator_received = new bool(false);

        return;
    }

    // If the env variables exist, we just read them & initialize our members
    ReadStoredEnvironmentVariable(counter_env_var_res, env_ptr, m_instance_counter);
    ReadStoredEnvironmentVariable(counter_mtx_env_var_res, env_ptr, m_counter_mtx);
    ReadStoredEnvironmentVariable(session_id_env_var_res, env_ptr, m_plugin_instance_id);
    ReadStoredEnvironmentVariable(session_flag_env_var_res, env_ptr, m_identificator_received);

    IncreaseCounter();
}

void PluginInstanceSynchronizer::IncreaseCounter() {
    std::scoped_lock scoped_lock(*m_counter_mtx);
    (*m_instance_counter)++;
}

PluginInstanceSynchronizer::~PluginInstanceSynchronizer() {
    bool ShouldFinalizeDestruction = false;

    {
        std::scoped_lock scoped_lock(*m_counter_mtx);
        if (*m_instance_counter > 1) {
            (*m_instance_counter)--;
        } else {
            ShouldFinalizeDestruction = true;

            m_env_handler.RemoveEnvVariable(EnvironmentVariableMutex);
            m_env_handler.RemoveEnvVariable(EnvironmentVariableCounter);
            m_env_handler.RemoveEnvVariable(EnvironmentVariableSessionIdentifier);
            m_env_handler.RemoveEnvVariable(EnvironmentVariableSessionFlag);

            // TODO: convert these into smart ptrs
            delete m_instance_counter;
            m_instance_counter = nullptr;
        }
    }

    if (ShouldFinalizeDestruction) {
        // Use smart ownership to prevent any leaks
        // We can also use a reference counted storage (i.e. a file with number of things depending on this)
        delete m_counter_mtx;
        m_counter_mtx = nullptr;
    }
}

void PluginInstanceSynchronizer::DecreaseCounter() {
    std::scoped_lock scoped_lock(*m_counter_mtx);
    if (*m_instance_counter > 0) {
        (*m_instance_counter)--;
    }
}

PluginInstanceSynchronizer *PluginInstanceSynchronizer::synchronisator = nullptr;

PluginInstanceSynchronizer *PluginInstanceSynchronizer::GetInstance() {
    if (synchronisator == nullptr) {
        synchronisator = new PluginInstanceSynchronizer();
    }
    synchronisator->IncreaseCounter();
    return synchronisator;
}


void PluginInstanceSynchronizer::SetInstanceIdentificationDecorator(const std::string &string) {
    std::array<char, 128> buffer{};
    snprintf(buffer.data(), buffer.size(), "%s", string.c_str());
    *m_plugin_instance_id = std::string(buffer.data());

    std::array<char, 128> env_ptr_buf{};
    snprintf(env_ptr_buf.data(), env_ptr_buf.size(), "%p", m_plugin_instance_id);
    m_env_handler.SetEnvVariable(EnvironmentVariableSessionIdentifier, env_ptr_buf.data());

    // Set the flag & save it
    *m_identificator_received = true;
    snprintf(env_ptr_buf.data(), env_ptr_buf.size(), "%p", m_identificator_received);
    m_env_handler.SetEnvVariable(EnvironmentVariableSessionFlag, env_ptr_buf.data());
}

std::string PluginInstanceSynchronizer::GetPluginSessionId() {
    return *m_plugin_instance_id;
}

size_t PluginInstanceSynchronizer::GetPluginInstantiationIndex() {
    return *m_instance_counter;
}

bool PluginInstanceSynchronizer::IsIdentifierReceived() const {
    return *m_identificator_received;
}
