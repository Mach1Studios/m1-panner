#ifndef M1_PANNER_DAWSYNCHRONISATOR_H
#define M1_PANNER_DAWSYNCHRONISATOR_H

#include <mutex>

#include "EnvironmentVariable.h"

/**
 * Class used to keep track of Panner instances, handing over instantiation index to each one of them
 *
 * A singleton pattern is utilized, which allows for all plugin instantiations to
 */
class PluginInstanceSynchronizer {
public:
    virtual ~PluginInstanceSynchronizer();

    PluginInstanceSynchronizer();

    PluginInstanceSynchronizer(PluginInstanceSynchronizer &) = delete;

    void operator=(const PluginInstanceSynchronizer &) = delete;

    /**
     * When the DAW Synchronizator is used as a singleton  
     * @return
     */
    static PluginInstanceSynchronizer *GetInstance();

    /**
     * Increases the global instance counter
     */
    void IncreaseCounter();

    /**
     * Decreases the global instance counter
     */
    void DecreaseCounter();

    /**
     * Set a unique identifier
     * @param string
     */
    void SetInstanceIdentificationDecorator(const std::string &string);

    /**
     *
     * @return
     */
    std::string GetPluginSessionId();

    /**
     * Returns the latest instantiated index observed by the DAWSynchronizer
     */
    size_t GetPluginInstantiationIndex();

    bool IsIdentifierReceived() const;

private:
    static PluginInstanceSynchronizer *synchronisator;

    // These variables' addresses are kept as environment variables
    // This is done in order for subsequent instances to be able to read & manipulate them
    std::mutex *m_counter_mtx = nullptr;
    size_t *m_instance_counter = nullptr;
    std::string *m_plugin_instance_id = nullptr;
    bool *m_identificator_received = nullptr;

    GA::EnvironmentHandler m_env_handler;

    // TODO: Get all keys for the environs, their ptrs, etc into a single container & manage them the same way
    std::string EnvironmentVariableCounter = "cntr",
            EnvironmentVariableMutex = "mtx",
            EnvironmentVariableSessionIdentifier = "session_id",
            EnvironmentVariableSessionFlag = "session_flag";
};

#endif //M1_PANNER_DAWSYNCHRONISATOR_H
