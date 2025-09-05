window.onload = function() {

  window.ui = SwaggerUIBundle({
    url: "./AMCF_OpenAPI.json",
    dom_id: '#swagger-ui',
    deepLinking: true,
    presets: [
      SwaggerUIBundle.presets.apis,
      SwaggerUIStandalonePreset
    ],
    plugins: [      
    ],
    layout: "StandaloneLayout"
  });

  //</editor-fold>
};
