"""Exact owned WebAudio source edits; preserve and reverse every unrelated upstream byte."""

import hashlib

PINS = {
    "audio_context.rs": "b65cec7b389be677923c9d41503a1fb362b79665d3444520d043605d3ba0fc52",
    "audio_scheduled_source_node.rs": "b1d5613e8a9e5b3b36df51a411961632ebc4de17d8509001a4c142b3d4bd0e6c",
    "audio_buffer_source_node.rs": "a3965d72eb702b4a6fff697b3d5f732fc7c64bb6bc786dd5edeb4de8c0d5abb7",
}

METHODS = """    // Only constructor/resume/source-start may acquire a native authorization.
    // No Window activation observer resumes contexts behind the caller's back.
    fn may_start(&self) -> bool {
        if self.inactive.get() || self.context.control_state() == AudioContextState::Closed {
            return false;
        }
        let mut authorization = self.authorization.get();
        let allowed = unsafe {
            firefly_web_audio_may_start(self.context.global_event_target().as_ptr(), &mut authorization)
        };
        self.authorization.set(authorization);
        allowed
    }

    fn validate_queued_start(&self, authorization: u64) -> bool {
        if self.inactive.get() || self.context.control_state() == AudioContextState::Closed {
            return false;
        }
        let global = self.context.global_event_target().as_ptr();
        if unsafe { firefly_web_audio_still_authorized(global, authorization) } {
            return true;
        }
        // A newer explicit resume may already have legitimately admitted this
        // same graph under a newer policy. Discard the old task, not that graph.
        let current = self.authorization.get();
        if current != authorization && unsafe { firefly_web_audio_still_authorized(global, current) } {
            return false;
        }
        self.document_became_inactive();
        self.context.set_control_state(AudioContextState::Suspended);
        self.context.set_rendering_state(AudioContextState::Suspended);
        if self.context.state_value() != AudioContextState::Suspended {
            self.context.set_state(AudioContextState::Suspended);
            with_temporary_execution_context_and_callbacks(self.context.realm(), || {
                self.context.dispatch_statechange();
            });
        }
        false
    }

    pub(crate) fn notify_source_start(&self, vm: &Vm) {
        if !self.may_start() || self.suspended_by_user.get() {
            return;
        }
        if self.context.control_state() == AudioContextState::Suspended {
            self.context.set_control_state(AudioContextState::Running);
            self.run_control_message_to_resume(vm, None);
        }
    }

    fn reject_blocked_resumes(&self, vm: &Vm) {
        // Keep the traced resume list intact until every promise is settled;
        // promise construction/rejection may allocate and collect.
        for promise in &*self.pending_resume_promises.borrow() {
            with_temporary_execution_context_and_callbacks(promise_realm(*promise), || {
                reject_promise_with_exception(vm, *promise, Exception::Dom(
                    "InvalidStateError", Utf16String::from_ascii(b"Audio context is no longer active"),
                ));
            });
            self.context.pending_promises.borrow_mut(vm).retain(|pending| pending != promise);
        }
        self.pending_resume_promises.borrow_mut(vm).clear();
    }

    fn start_rendering_audio_graph(&self, vm: &Vm) -> bool {
        if !self.may_start() {
            return false;
        }
        if self.renderer.borrow().is_none() {
            // Admission precedes even creating the native factory. Blocked
            // contexts own no renderer/client root or audio output resources.
            let factory = unsafe { web_audio_output_factory_create(self.context.global_event_target().as_ptr()) };
            let renderer = RealtimeAudioRenderer::new(
                Arc::clone(self.context.control_message_queue()),
                self.context.destination_value().parent().node_id(),
                self.context.sample_rate_value(),
                BaseAudioContext::RENDER_QUANTUM_SIZE as usize,
                SharedCppObject { pointer: factory.pointer, retain: factory.retain, release: factory.release },
            );
            *self.renderer.borrow_mut() = Some(Arc::clone(&renderer));
            self.set_renderer_client(vm);
            renderer.start_rendering();
        } else {
            self.set_renderer_client(vm);
            self.renderer.borrow().as_ref().unwrap().resume();
        }
        true
    }

"""


def compose(name, source):
    if name not in PINS or hashlib.sha256(source.encode()).hexdigest() != PINS[name]:
        raise ValueError(f"Review pinned WebAudio source: {name}")
    changes = []

    def replace(before, after):
        nonlocal source
        if source.count(before) != 1:
            raise ValueError(f"Missing/duplicate WebAudio anchor in {name}: {before[:80]}")
        source = source.replace(before, after)
        changes.append((before, after))

    if name != "audio_context.rs":
        number = "5" if name == "audio_scheduled_source_node.rs" else "6"
        old = f"        // FIXME: {number}. Send a control message to the associated AudioContext to start running its rendering thread only when all the following conditions are met:"
        context = (
            "self.node.context_value()" if number == "5" else "self.node.parent().context_value()"
        )
        replace(
            old,
            f"""        // Realtime contexts alone perform native autoplay admission; offline
        // contexts retain their existing explicit startRendering lifecycle.
        if let AnyBaseAudioContext::AudioContext(context) = {context} {{
            context.notify_source_start(cx.vm());
        }}""",
        )
    else:
        replace(
            "    allowed_to_start: bool,",
            """    authorization: Cell<u64>,
    inactive: Cell<bool>,
    // SAFETY: the owning VM outlives its embedder cells and document callbacks.
    #[gc(untraced)]
    vm: NonNull<Vm>,""",
        )
        replace(
            "    fn web_audio_output_factory_create(global: *mut c_void) -> FfiSharedCppObject;",
            """    fn firefly_web_audio_may_start(global: *mut c_void, authorization: *mut u64) -> bool;
    fn firefly_web_audio_still_authorized(global: *mut c_void, authorization: u64) -> bool;
    fn web_audio_output_factory_create(global: *mut c_void) -> FfiSharedCppObject;""",
        )
        replace(
            "        let context = allocate(cx.vm(), |defer_gc| Self {",
            """        let vm = NonNull::from(cx.vm());
        let context = allocate(cx.vm(), |defer_gc| Self {""",
        )
        replace(
            "            allowed_to_start: true,",
            """            authorization: Cell::new(0),
            inactive: Cell::new(false),
            vm,""",
        )
        begin = source.index("        // FIXME: The context is currently always allowed to start;")
        end = source.index("            // 2. Set this [[rendering thread state]]", begin)
        replace(
            source[begin:end],
            """        if context.may_start() && context.start_rendering_audio_graph(cx.vm()) {
            context.context.set_control_state(AudioContextState::Running);
""",
        )
        replace(
            ".queue_a_media_element_task(cx.vm(), context, |_, context| {",
            ".queue_a_media_element_task(cx.vm(), (context, context.authorization.get()), |_, (context, authorization)| {",
        )
        replace(
            """                    // 1. Set the state attribute of the AudioContext to "running".
""",
            """                    if context.inactive.get() || context.context.control_state() != AudioContextState::Running
                        || !context.validate_queued_start(authorization) {
                        return;
                    }
                    // 1. Set the state attribute of the AudioContext to "running".
""",
        )
        replace(
            """    pub(crate) fn document_became_inactive(&self) {
""",
            """    pub(crate) fn document_became_inactive(&self) {
        self.inactive.set(true);
        // SAFETY: document teardown runs while the owning VM is alive.
        self.reject_blocked_resumes(unsafe { self.vm.as_ref() });
""",
        )
        replace(
            'pub(crate) fn resume(&self, cx: &mut CallContext, promise: WebIdlPromise) -> WebIdlResult<()> {\n        // 1. If this’s relevant global object’s associated Document is not fully active then return a promise rejected with "InvalidStateError" DOMException.\n        if !self.context.document_is_fully_active() {',
            'pub(crate) fn resume(&self, cx: &mut CallContext, promise: WebIdlPromise) -> WebIdlResult<()> {\n        // 1. If this’s relevant global object’s associated Document is not fully active then return a promise rejected with "InvalidStateError" DOMException.\n        if self.inactive.get() || !self.context.document_is_fully_active() {',
        )
        replace("        if !self.allowed_to_start {", "        if !self.may_start() {")
        replace(
            "        self.run_control_message_to_resume(cx.vm(), promise);",
            "        self.run_control_message_to_resume(cx.vm(), Some(promise));",
        )
        replace(
            "    fn run_control_message_to_resume(&self, vm: &Vm, promise: WebIdlPromise) {",
            "    fn run_control_message_to_resume(&self, vm: &Vm, promise: Option<WebIdlPromise>) {",
        )
        begin = source.index("        // 2. Attempt to acquire system resources.\n")
        end = source.index("        // 5. In case of failure,", begin)
        replace(
            source[begin:end],
            """        // Acquire/start a real graph before publishing Running.
        let started = self.start_rendering_audio_graph(vm);
        if started {
            self.context.set_rendering_state(AudioContextState::Running);
        }

""",
        )
        begin = source.index("        if !started {\n")
        end = source.index("        // 6. queue a media element task", begin)
        replace(
            source[begin:end],
            """        if !started {
            self.context.queue_a_media_element_task(vm, (self.this(), promise), |vm, (context, promise)| {
                context.reject_blocked_resumes(vm);
                if let Some(promise) = promise {
                    with_temporary_execution_context_and_callbacks(promise_realm(promise), || {
                        reject_promise_with_exception(vm, promise, Exception::Dom(
                            "InvalidStateError", Utf16String::from_ascii(b"Audio context cannot start"),
                        ));
                    });
                }
            });
            return;
        }

""",
        )
        replace(
            "        // 6. queue a media element task to execute the following steps:\n        self.context\n            .queue_a_media_element_task(vm, (self.this(), promise), |vm, (context, promise)| {",
            "        // 6. queue a media element task to execute the following steps:\n        self.context\n            .queue_a_media_element_task(vm, (self.this(), promise, self.authorization.get()), |vm, (context, promise, authorization)| {",
        )
        replace(
            """                // 6.1. If the [[render quantum size]]""",
            """                if !context.validate_queued_start(authorization) {
                    if context.context.control_state() == AudioContextState::Closed {
                        context.reject_blocked_resumes(vm);
                    }
                    if let Some(promise) = promise {
                        with_temporary_execution_context_and_callbacks(promise_realm(promise), || {
                            reject_promise_with_exception(vm, promise, Exception::Dom(
                                "InvalidStateError", Utf16String::from_ascii(b"Audio context is no longer active"),
                            ));
                        });
                    }
                    return;
                }
                // 6.1. If the [[render quantum size]]""",
        )
        replace(
            """                with_temporary_execution_context_and_callbacks(promise_realm(promise), || {
                    // 6.4. Resolve promise.
                    resolve_promise(vm, promise, Value::UNDEFINED);

                    // 6.5.""",
            """                if let Some(promise) = promise {
                    with_temporary_execution_context_and_callbacks(promise_realm(promise), || {
                        resolve_promise(vm, promise, Value::UNDEFINED);
                    });
                }
                // A later explicit suspend must not be undone by an older task.
                if context.context.control_state() == AudioContextState::Running {
                    // 6.5.""",
        )
        replace(
            """                });
            });
    }

    // https://webaudio.github.io/web-audio-api/#dom-audiocontext-suspend""",
            """                }
            });
    }

    // https://webaudio.github.io/web-audio-api/#dom-audiocontext-suspend""",
        )
        replace(
            "        self.context.set_control_state(AudioContextState::Closed);",
            "        self.context.set_control_state(AudioContextState::Closed);\n        self.reject_blocked_resumes(cx.vm());",
        )
        begin = source.index("    fn start_rendering_audio_graph(&self) -> bool {")
        end = source.index("    // The renderer's callback", begin)
        replace(source[begin:end], METHODS)
        # Rejection now always carries a genuine InvalidStateError.
        replace(
            "    WebIdlPromise, promise_realm, reject_promise, reject_promise_with_exception, resolve_promise,",
            "    WebIdlPromise, promise_realm, reject_promise_with_exception, resolve_promise,",
        )
    restored = source
    for before, after in reversed(changes):
        if restored.count(after) != 1:
            raise ValueError(f"Ambiguous WebAudio reversal in {name}")
        restored = restored.replace(after, before, 1)
    if hashlib.sha256(restored.encode()).hexdigest() != PINS[name]:
        raise ValueError(f"WebAudio changed bytes outside reviewed edits: {name}")
    return source
