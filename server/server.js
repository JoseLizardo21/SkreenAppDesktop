const express = require('express');
const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 9001 });

let cppClient = null;
let flutterClient = null;

// ========== NUEVO: Almacenar última oferta y ICE candidates ==========
let pendingOffer = null;
let pendingIceCandidates = [];

wss.on('connection', (ws) => {
    ws.on('message', (msg) => {
        const text = msg.toString();

        // Registro de clientes
        if (text === "register_cpp") {
            cppClient = ws;
            console.log("✅ Cliente C++ registrado");
            return;
        }

        if (text === "register_flutter") {
            flutterClient = ws;
            console.log("✅ Cliente Flutter registrado");

            // ========== NUEVO: Enviar oferta pendiente ==========
            if (pendingOffer) {
                console.log("📤 Enviando oferta pendiente a Flutter");
                flutterClient.send(pendingOffer);
                pendingOffer = null;
            }

            // Enviar ICE candidates pendientes
            if (pendingIceCandidates.length > 0) {
                console.log(`📤 Enviando ${pendingIceCandidates.length} ICE candidates pendientes`);
                pendingIceCandidates.forEach(candidate => {
                    flutterClient.send(candidate);
                });
                pendingIceCandidates = [];
            }

            // ========== NUEVO: Notificar a C++ que Flutter está listo ==========
            if (cppClient) {
                console.log("📤 Notificando a C++ que Flutter está listo");
                const readyMsg = JSON.stringify({ type: 'flutter_ready' });
                cppClient.send(readyMsg);
            }

            return;
        }

        // Señalización WebRTC (SDP y ICE candidates)
        try {
            const data = JSON.parse(text);

            // Reenviar señalización de C++ → Flutter
            if (ws === cppClient) {
                console.log("📤 C++ → Flutter:", data.type);

                if (flutterClient) {
                    flutterClient.send(text);
                } else {
                    // ========== NUEVO: Almacenar si Flutter no está conectado ==========
                    if (data.type === 'offer') {
                        console.log("💾 Almacenando oferta (Flutter aún no conectado)");
                        pendingOffer = text;
                    } else if (data.type === 'candidate') {
                        console.log("💾 Almacenando ICE candidate");
                        pendingIceCandidates.push(text);
                    }
                }
            }

            // Reenviar señalización de Flutter → C++
            if (ws === flutterClient && cppClient) {
                console.log("📤 Flutter → C++:", data.type);
                cppClient.send(text);
            }
        } catch (e) {
            console.error("Error parseando mensaje:", e);
        }
    });

    ws.on('close', () => {
        if (ws === cppClient) {
            console.log("❌ Cliente C++ desconectado");
            cppClient = null;
            // Limpiar mensajes pendientes
            pendingOffer = null;
            pendingIceCandidates = [];
        }
        if (ws === flutterClient) {
            console.log("❌ Cliente Flutter desconectado");
            flutterClient = null;
        }
    });
});

console.log("🚀 Servidor WebRTC Signaling en puerto 9001");